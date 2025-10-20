#include "Utils/ta_simple.h"//

namespace TA {

static inline bool isFinite(double x) { return qIsFinite(x); }

// --- SMA ---
QVector<double> sma(const QVector<double>& v, int period, int warmup) {
    const int n = v.size();
    QVector<double> out(n, NaN());
    if (period <= 0 || n == 0) return out;

    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < n; ++i) {
        double x = v[i];
        if (isFinite(x)) { sum += x; ++count; }
        if (i >= period) {
            double xold = v[i - period];
            if (isFinite(xold)) { sum -= xold; --count; }
        }
        if (i + 1 >= period && count == period) out[i] = sum / period;
        if (warmup > 0 && i + 1 < warmup) out[i] = NaN();
    }
    return out;
}

// --- EMA ---
QVector<double> ema(const QVector<double>& v, int period, int warmup) {
    const int n = v.size();
    QVector<double> out(n, NaN());
    if (period <= 0 || n == 0) return out;

    const double k = 2.0 / (period + 1.0);
    double prev = NaN();
    int seedCount = 0;
    double seedSum = 0.0;

    for (int i = 0; i < n; ++i) {
        double x = v[i];
        if (!isFinite(x)) { out[i] = NaN(); continue; }

        if (!isFinite(prev)) {
            // SMA seed
            seedSum += x;
            ++seedCount;
            if (seedCount >= period) {
                prev = seedSum / period;
                out[i] = prev;
            } else {
                out[i] = NaN();
            }
        } else {
            prev = x * k + prev * (1.0 - k);
            out[i] = prev;
        }
        if (warmup > 0 && i + 1 < warmup) out[i] = NaN();
    }
    return out;
}

// --- StdDev (population) ---
QVector<double> stddev(const QVector<double>& v, int period, int warmup) {
    const int n = v.size();
    QVector<double> out(n, NaN());
    if (period <= 1 || n == 0) return out;

    double sum = 0.0, sum2 = 0.0;
    int count = 0;
    auto add = [&](double x){ sum += x; sum2 += x*x; ++count; };
    auto sub = [&](double x){ sum -= x; sum2 -= x*x; --count; };

    for (int i = 0; i < n; ++i) {
        double x = v[i];
        if (isFinite(x)) add(x);
        if (i >= period) {
            double xold = v[i - period];
            if (isFinite(xold)) sub(xold);
        }
        if (i + 1 >= period && count == period) {
            double mean = sum / period;
            double var = (sum2 / period) - (mean * mean);
            out[i] = var > 0 ? qSqrt(var) : 0.0;
        }
        if (warmup > 0 && i + 1 < warmup) out[i] = NaN();
    }
    return out;
}

// --- Bollinger Bands ---
BBands bollinger(const QVector<double>& close, int period, double stdevMult, int warmup) {
    BBands bb;
    bb.mid = sma(close, period, warmup);
    QVector<double> dev = stddev(close, period, warmup);
    const int n = close.size();
    bb.upper.resize(n);
    bb.lower.resize(n);
    for (int i = 0; i < n; ++i) {
        double m = bb.mid.value(i, NaN());
        double s = dev.value(i, NaN());
        if (isFinite(m) && isFinite(s)) {
            bb.upper[i] = m + stdevMult * s;
            bb.lower[i] = m - stdevMult * s;
        } else {
            bb.upper[i] = NaN();
            bb.lower[i] = NaN();
        }
    }
    return bb;
}

// --- Stochastics ---
Stoch stochastics(const QVector<double>& high,
                  const QVector<double>& low,
                  const QVector<double>& close,
                  int kPeriod, int kSmoothing, int dPeriod, int warmup)
{
    const int n = close.size();
    Stoch st;
    st.fastK.fill(NaN(), n);
    QVector<double> slowK(n, NaN());
    QVector<double> slowD(n, NaN());

    if (n == 0 || kPeriod <= 0) {
        st.k = slowK; st.d = slowD;
        return st;
    }

    QVector<double> rollingHigh(n, NaN());
    QVector<double> rollingLow(n, NaN());

    for (int i = 0; i < n; ++i) {
        int start = qMax(0, i - kPeriod + 1);
        double hh = -std::numeric_limits<double>::infinity();
        double ll =  std::numeric_limits<double>::infinity();
        for (int j = start; j <= i; ++j) {
            hh = qMax(hh, high.value(j, NaN()));
            ll = qMin(ll,  low.value(j, NaN()));
        }
        rollingHigh[i] = hh;
        rollingLow[i]  = ll;

        double denom = hh - ll;
        if (qFuzzyIsNull(denom) || !isFinite(denom)) {
            st.fastK[i] = NaN();
        } else {
            st.fastK[i] = 100.0 * (close[i] - ll) / denom;
        }
    }

    slowK = sma(st.fastK, kSmoothing, warmup);
    slowD = sma(slowK, dPeriod, warmup);

    st.k = slowK;
    st.d = slowD;
    return st;
}

// --- VWAP (intraday reset) ---
QVector<double> vwap(const QVector<double>& high,
                     const QVector<double>& low,
                     const QVector<double>& close,
                     const QVector<double>& volume,
                     const QVector<QDateTime>& ts)
{
    const int n = close.size();
    QVector<double> out(n, NaN());
    if (n == 0) return out;

    double cumPV = 0.0;
    double cumVol = 0.0;
    QDate curDate;

    for (int i = 0; i < n; ++i) {
        QDate d = ts.value(i).date();
        if (curDate != d) { // reset on date change
            curDate = d; cumPV = 0.0; cumVol = 0.0;
        }
        double typical = (high.value(i, NaN()) + low.value(i, NaN()) + close.value(i, NaN())) / 3.0;
        double vol = volume.value(i, NaN());
        if (!isFinite(typical) || !isFinite(vol) || vol <= 0) {
            out[i] = (cumVol > 0.0) ? (cumPV / cumVol) : NaN();
            continue;
        }
        cumPV  += typical * vol;
        cumVol += vol;
        out[i] = (cumVol > 0.0) ? (cumPV / cumVol) : NaN();
    }
    return out;
}

// --- Pivot Point sets ---
Pivots pivotsClassic(double H, double L, double C) {
    Pivots p{};
    p.P  = (H + L + C) / 3.0;
    double R = H - L;
    p.R1 = 2*p.P - L;  p.S1 = 2*p.P - H;
    p.R2 = p.P + R;    p.S2 = p.P - R;
    p.R3 = H + 2*(p.P - L);  p.S3 = L - 2*(H - p.P);
    p.R4 = p.R3 + R;   p.S4 = p.S3 - R;
    p.R5 = p.R4 + R;   p.S5 = p.S4 - R;
    return p;
}

Pivots pivotsFibonacci(double H, double L, double C) {
    Pivots p{};
    p.P = (H + L + C) / 3.0;
    double R = H - L;
    p.R1 = p.P + 0.382 * R;  p.S1 = p.P - 0.382 * R;
    p.R2 = p.P + 0.618 * R;  p.S2 = p.P - 0.618 * R;
    p.R3 = p.P + 1.000 * R;  p.S3 = p.P - 1.000 * R;
    p.R4 = p.P + 1.272 * R;  p.S4 = p.P - 1.272 * R;
    p.R5 = p.P + 1.618 * R;  p.S5 = p.P - 1.618 * R;
    return p;
}

Pivots pivotsCamarilla(double H, double L, double C) {
    Pivots p{};
    p.P = (H + L + C) / 3.0;
    double R = H - L;
    p.R1 = C + (R * 1.1 / 12.0);  p.S1 = C - (R * 1.1 / 12.0);
    p.R2 = C + (R * 1.1 /  6.0);  p.S2 = C - (R * 1.1 /  6.0);
    p.R3 = C + (R * 1.1 /  4.0);  p.S3 = C - (R * 1.1 /  4.0);
    p.R4 = C + (R * 1.1 /  2.0);  p.S4 = C - (R * 1.1 /  2.0);
    p.R5 = C + R;                 p.S5 = C - R;
    return p;
}

QVector<double> slice(const QVector<double>& v, int start, int end) {
    QVector<double> out;
    if (start < 0) start = 0;
    if (end < 0 || end > v.size()) end = v.size();
    if (start >= end) return out;
    out.reserve(end - start);
    for (int i = start; i < end; ++i) out.push_back(v[i]);
    return out;
}

} // namespace TA
