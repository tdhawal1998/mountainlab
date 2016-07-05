/******************************************************
** See the accompanying README and LICENSE files
** Author(s): Jeremy Magland
** Created: 5/27/2016
*******************************************************/

#include "mvfiringeventview2.h"
#include <math.h>

#include <QImageWriter>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QThread>
#include <taskprogress.h>
#include "mvclusterlegend.h"
#include "paintlayerstack.h"

/// TODO: (MEDIUM) control brightness in firing event view

class MVFiringEventViewCalculator {
public:
    //input
    DiskReadMda firings;
    MVEventFilter filter;
    QSet<int> labels_to_use;

    //output
    QVector<double> times;
    QVector<int> labels;
    QVector<double> amplitudes;

    void compute();
};

class FiringEventAxisLayer : public PaintLayer {
public:
    void paint(QPainter* painter) Q_DECL_OVERRIDE;

    QRectF content_geometry;
    MVRange amplitude_range;
};

class FiringEventContentLayer;
class FiringEventImageCalculator : public QThread {
public:
    void run();

    /// TODO only calculate the image in the content geometry rect so that we don't need things like window_size and the image may be safely resized on resizing the window

    //input
    QList<QColor> cluster_colors;
    QRectF content_geometry;
    QSize window_size;
    QVector<double> times;
    QVector<int> labels;
    QVector<double> amplitudes;
    MVRange time_range;
    MVRange amplitude_range;

    //output
    QImage image;

    QColor cluster_color(int k);
    double time2xpix(double t);
    double val2ypix(double val);
};

class MVFiringEventView2Private {
public:
    MVFiringEventView2* q;
    //DiskReadMda m_timeseries;

    MVRange m_amplitude_range;
    QSet<int> m_labels_to_use;
    QVector<double> m_times0;
    QVector<int> m_labels0;
    QVector<double> m_amplitudes0;

    FiringEventContentLayer* m_content_layer;
    MVClusterLegend* m_legend;

    MVFiringEventViewCalculator m_calculator;

    PaintLayerStack m_paint_layer_stack;
    FiringEventAxisLayer* m_axis_layer;

    double val2ypix(double val);
    double ypix2val(double ypix);
};

MVFiringEventView2::MVFiringEventView2(MVContext* context)
    : MVTimeSeriesViewBase(context)
{
    d = new MVFiringEventView2Private;
    d->q = this;

    this->setMouseTracking(true);

    d->m_legend = new MVClusterLegend;
    d->m_legend->setClusterColors(context->clusterColors());

    d->m_content_layer = new FiringEventContentLayer;

    d->m_axis_layer = new FiringEventAxisLayer;
    d->m_paint_layer_stack.addLayer(d->m_content_layer);
    d->m_paint_layer_stack.addLayer(d->m_axis_layer);
    d->m_paint_layer_stack.addLayer(d->m_legend);
    connect(&d->m_paint_layer_stack, SIGNAL(repaintNeeded()), this, SLOT(update()));

    d->m_amplitude_range = MVRange(0, 1);
    this->setMarkersVisible(false);
    this->setMargins(60, 60, 40, 40);

    mvtsv_prefs p = this->prefs();
    p.colors.axis_color = Qt::white;
    p.colors.text_color = Qt::white;
    p.colors.background_color = Qt::black;
    this->setPrefs(p);

    connect(context, SIGNAL(currentTimeRangeChanged()), SLOT(slot_update_image()));

    this->recalculateOn(context, SIGNAL(clusterMergeChanged()));
    this->recalculateOn(context, SIGNAL(filteredFiringsChanged()));
    this->recalculate();
}

MVFiringEventView2::~MVFiringEventView2()
{
    delete d;
}

void MVFiringEventView2::prepareCalculation()
{
    d->m_calculator.labels_to_use = d->m_labels_to_use;
    d->m_calculator.firings = mvContext()->firings();
    d->m_calculator.filter = mvContext()->eventFilter();
}

void MVFiringEventView2::runCalculation()
{
    d->m_calculator.compute();
}

void MVFiringEventView2::onCalculationFinished()
{
    d->m_labels0 = d->m_calculator.labels;
    d->m_times0 = d->m_calculator.times;
    d->m_amplitudes0 = d->m_calculator.amplitudes;

    d->m_labels0 = this->mvContext()->clusterMerge().mapLabels(d->m_labels0);

    {
        QSet<int> X;
        foreach (int k, d->m_labels_to_use) {
            X.insert(this->mvContext()->clusterMerge().representativeLabel(k));
        }
        QList<int> list = X.toList();
        qSort(list);
        d->m_legend->setClusterNumbers(list);
    }

    slot_update_image();
    /// TODO: (MEDIUM) only do this if user has specified that it should be auto calculated (should be default)
    this->autoSetAmplitudeRange();
}

void MVFiringEventView2::setLabelsToUse(const QSet<int>& labels_to_use)
{
    d->m_labels_to_use = labels_to_use;
    this->recalculate();
}

void MVFiringEventView2::setAmplitudeRange(MVRange range)
{
    if (d->m_amplitude_range == range)
        return;
    d->m_amplitude_range = range;
    d->m_axis_layer->amplitude_range = range;
    update();

    slot_update_image(); //because amplitude range has changed
}

#include "msmisc.h"
#include "mvmainwindow.h"
void MVFiringEventView2::autoSetAmplitudeRange()
{
    double min0 = compute_min(d->m_amplitudes0);
    double max0 = compute_max(d->m_amplitudes0);
    setAmplitudeRange(MVRange(qMin(0.0, min0), qMax(0.0, max0)));
}

void MVFiringEventView2::mouseMoveEvent(QMouseEvent* evt)
{
    d->m_paint_layer_stack.mouseMoveEvent(evt);
    if (evt->isAccepted())
        return;
    MVTimeSeriesViewBase::mouseMoveEvent(evt);
    /*
    int k=d->m_legend.clusterNumberAt(evt->pos());
    if (d->m_legend.hoveredClusterNumber()!=k) {
        d->m_legend.setHoveredClusterNumber(k);
        update();
    }
    */
}

void MVFiringEventView2::mouseReleaseEvent(QMouseEvent* evt)
{
    d->m_paint_layer_stack.mouseReleaseEvent(evt);
    if (evt->isAccepted())
        return;
    /*
    int k=d->m_legend.clusterNumberAt(evt->pos());
    if (k>0) {
        /// TODO (LOW) make the legend more like a widget, responding to mouse clicks and movements on its own, and emitting signals
        d->m_legend.toggleActiveClusterNumber(k);
        evt->ignore();
        update();
    }
    */
    MVTimeSeriesViewBase::mouseReleaseEvent(evt);
}

void MVFiringEventView2::resizeEvent(QResizeEvent* evt)
{
    d->m_axis_layer->content_geometry = this->contentGeometry();
    d->m_paint_layer_stack.setWindowSize(this->size());
    slot_update_image();
    MVTimeSeriesViewBase::resizeEvent(evt);
}

void MVFiringEventView2::slot_update_image()
{
    d->m_content_layer->calculator->requestInterruption();
    d->m_content_layer->calculator->wait();
    d->m_content_layer->calculator->cluster_colors = this->mvContext()->clusterColors();
    d->m_content_layer->calculator->times = d->m_times0;
    d->m_content_layer->calculator->labels = d->m_labels0;
    d->m_content_layer->calculator->amplitudes = d->m_amplitudes0;
    d->m_content_layer->calculator->content_geometry = this->contentGeometry();
    d->m_content_layer->calculator->window_size = this->size();
    d->m_content_layer->calculator->time_range = this->mvContext()->currentTimeRange();
    d->m_content_layer->calculator->amplitude_range = this->d->m_amplitude_range;
    d->m_content_layer->calculator->start();
}

void MVFiringEventView2::paintContent(QPainter* painter)
{
    d->m_paint_layer_stack.paint(painter);

    //legend
    //d->m_legend.setParentWindowSize(this->size());
    //d->m_legend.draw(painter);
    /*
    {
        double spacing = 6;
        double margin = 10;
        // it would still be better if m_labels.was presorted right from the start
        QList<int> list = d->m_labels_to_use.toList();
        qSort(list);
        double text_height = qBound(12.0, width() * 1.0 / 10, 25.0);
        double y0 = margin;
        QFont font = painter->font();
        font.setPixelSize(text_height - 1);
        painter->setFont(font);
        for (int i = 0; i < list.count(); i++) {
            QRectF rect(0, y0, width() - margin, text_height);
            QString str = QString("%1").arg(list[i]);
            QPen pen = painter->pen();
            pen.setColor(mvContext()->clusterColor(list[i]));
            painter->setPen(pen);
            painter->drawText(rect, Qt::AlignRight, str);
            y0 += text_height + spacing;
        }
    }
    */
}

double MVFiringEventView2Private::val2ypix(double val)
{
    if (m_amplitude_range.min >= m_amplitude_range.max)
        return 0;
    double pcty = (val - m_amplitude_range.min) / (m_amplitude_range.max - m_amplitude_range.min);

    return q->contentGeometry().top() + (1 - pcty) * q->contentGeometry().height();
}

double MVFiringEventView2Private::ypix2val(double ypix)
{
    if (m_amplitude_range.min >= m_amplitude_range.max)
        return 0;

    double pcty = 1 - (ypix - q->contentGeometry().top()) / q->contentGeometry().height();
    return m_amplitude_range.min + pcty * (m_amplitude_range.max - m_amplitude_range.min);
}

void MVFiringEventViewCalculator::compute()
{
    TaskProgress task("Computing firing events");

    firings = compute_filtered_firings_locally(firings, filter);

    long L = firings.N2();
    times.clear();
    labels.clear();
    amplitudes.clear();
    for (long i = 0; i < L; i++) {
        if (i % 100 == 0) {
            if (thread_interrupt_requested()) {
                task.error("Halted");
                return;
            }
        }
        int label0 = (int)firings.value(2, i);
        if (labels_to_use.contains(label0)) {

            task.setProgress(i * 1.0 / L);
            times << firings.value(1, i);
            labels << label0;
            amplitudes << firings.value(3, i);
        }
    }
    task.log(QString("Found %1 events, using %2 clusters").arg(times.count()).arg(labels_to_use.count()));
}

MVFiringEventsFactory::MVFiringEventsFactory(MVContext* context, QObject* parent)
    : MVAbstractViewFactory(context, parent)
{
    connect(context, SIGNAL(selectedClustersChanged()),
        this, SLOT(updateEnabled()));
    updateEnabled();
}

QString MVFiringEventsFactory::id() const
{
    return QStringLiteral("open-firing-events");
}

QString MVFiringEventsFactory::name() const
{
    return tr("Firing Events");
}

QString MVFiringEventsFactory::title() const
{
    return tr("Firing Events");
}

MVAbstractView* MVFiringEventsFactory::createView(QWidget* parent)
{
    QList<int> ks = mvContext()->selectedClustersIncludingMerges();
    if (ks.isEmpty()) {
        QMessageBox::warning(0, "Unable to open firing events", "You must select at least one cluster.");
        return Q_NULLPTR;
    }
    MVFiringEventView2* X = new MVFiringEventView2(mvContext());
    X->setLabelsToUse(ks.toSet());
    X->setNumTimepoints(mvContext()->currentTimeseries().N2());
    return X;
}

void MVFiringEventsFactory::updateEnabled()
{
    //bool has_peaks = (d->m_firings.value(0, 3) != 0); //for now we just test the very first one (might be problematic)
    /// TODO: (0.9.1) restore this has_peaks without accessing m_firings in gui thread
    bool has_peaks = true;
    setEnabled(!mvContext()->selectedClusters().isEmpty() && has_peaks);
}

void FiringEventAxisLayer::paint(QPainter* painter)
{
    draw_axis_opts opts;
    opts.minval = amplitude_range.min;
    opts.maxval = amplitude_range.max;
    opts.orientation = Qt::Vertical;
    opts.pt1 = content_geometry.bottomLeft() + QPointF(-3, 0);
    opts.pt2 = content_geometry.topLeft() + QPointF(-3, 0);
    opts.tick_length = 5;
    opts.color = Qt::white;
    draw_axis(painter, opts);
}

FiringEventContentLayer::FiringEventContentLayer()
{
    calculator = new FiringEventImageCalculator;
    connect(calculator, SIGNAL(finished()), this, SLOT(slot_calculator_finished()));
}

FiringEventContentLayer::~FiringEventContentLayer()
{
    calculator->requestInterruption();
    calculator->wait();
    delete calculator;
}

void FiringEventContentLayer::paint(QPainter* painter)
{
    painter->drawImage(0, 0, output_image);
}

void FiringEventContentLayer::setWindowSize(QSize size)
{
    PaintLayer::setWindowSize(size);
}

void FiringEventContentLayer::updateImage()
{
    calculator->requestInterruption();
    calculator->wait();
    calculator->start();
}

void FiringEventContentLayer::slot_calculator_finished()
{
    if (this->calculator->isRunning()) {
        // Calculator still running even though we are in slot_caculator_finished (think about how this happens)
        return;
    }
    if (this->calculator->isInterruptionRequested())
        return;
    this->output_image = this->calculator->image;
    emit repaintNeeded();
}

void FiringEventImageCalculator::run()
{
    image = QImage(window_size, QImage::Format_ARGB32);
    QColor transparent(0, 0, 0, 0);
    image.fill(transparent);
    QPainter painter(&image);
    double alpha_pct = 0.7;
    for (long i = 0; i < times.count(); i++) {
        if (this->isInterruptionRequested())
            return;
        double t0 = times.value(i);
        int k0 = labels.value(i);
        QColor col = cluster_color(k0);
        col.setAlpha((int)(alpha_pct * 255));
        QPen pen = painter.pen();
        pen.setColor(col);
        painter.setPen(pen);
        double amp0 = amplitudes.value(i);
        double xpix = time2xpix(t0);
        double ypix = val2ypix(amp0);
        painter.drawEllipse(xpix, ypix, 3, 3);
    }
}

QColor FiringEventImageCalculator::cluster_color(int k)
{
    if (k <= 0)
        return Qt::white;
    if (cluster_colors.isEmpty())
        return Qt::white;
    return cluster_colors[(k - 1) % cluster_colors.count()];
}

/// TODO do not duplicate code in time2xpix and val2ypix, but beware of calculator thread
double FiringEventImageCalculator::time2xpix(double t)
{
    double view_t1 = time_range.min;
    double view_t2 = time_range.max;

    if (view_t2 <= view_t1)
        return 0;

    double xpct = (t - view_t1) / (view_t2 - view_t1);
    double px = content_geometry.x() + xpct * content_geometry.width();
    return px;
}

double FiringEventImageCalculator::val2ypix(double val)
{
    if (amplitude_range.min >= amplitude_range.max)
        return 0;
    double pcty = (val - amplitude_range.min) / (amplitude_range.max - amplitude_range.min);

    return content_geometry.top() + (1 - pcty) * content_geometry.height();
}
