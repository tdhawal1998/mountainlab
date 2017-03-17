#include "p_concat_firings.h"

#include <diskreadmda.h>
#include <diskreadmda32.h>
#include <diskwritemda.h>

bool p_concat_firings(QStringList timeseries_list, QStringList firings_list, QString timeseries_out, QString firings_out)
{
    DiskReadMda32 X0(timeseries_list.value(0));
    int M = X0.N1();
    DiskReadMda F0(firings_list.value(0));
    int R = F0.N1();

    int NN = 0;
    int LL = 0;
    for (int i = 0; i < firings_list.count(); i++) {
        if (!timeseries_list.isEmpty()) {
            DiskReadMda32 X1(timeseries_list.value(i));
            NN += X1.N2();
        }
        DiskReadMda F1(firings_list.value(i));
        LL += F1.N2();
    }

    DiskWriteMda Y;
    if (!timeseries_out.isEmpty()) {
        Y.open(X0.mdaioHeader().data_type, timeseries_out, M, NN);
    }
    Mda G(R, LL);
    bigint n0 = 0;
    bigint i0 = 0;
    for (int i = 0; i < firings_list.count(); i++) {
        printf("Firings %d of %d\n", i + 1, firings_list.count());
        Mda F1(firings_list.value(i));
        for (int j = 0; j < F1.N2(); j++) {
            for (int r = 0; r < R; r++) {
                G.setValue(F1.value(r, j), r, i0 + j);
            }
            G.setValue(G.value(1, i0 + j) + n0, 1, i0 + j);
        }

        if (!timeseries_list.isEmpty()) {
            DiskReadMda32 X1(timeseries_list.value(i));
            if (!timeseries_out.isEmpty()) {
                Mda32 chunk;
                X1.readChunk(chunk, 0, 0, M, X1.N2());
                Y.writeChunk(chunk, 0, n0);
            }
            n0 += X1.N2();
        }
        i0 += F1.N2();
    }

    G.write64(firings_out);

    return true;
}

bool p_concat_event_times(QStringList event_times_list, QString event_times_out)
{
    bigint NN = 0;
    foreach (QString fname, event_times_list) {
        DiskReadMda ET(fname);
        NN += ET.totalSize();
    }
    Mda ret(1, NN);
    bigint offset = 0;
    foreach (QString fname, event_times_list) {
        Mda ET(fname);
        ret.setChunk(ET, offset);
        offset += ET.totalSize();
    }
    return ret.write64(event_times_out);
}
