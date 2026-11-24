"""Train the route-draft net (src/game/navigation/planner/RouteDraftNet.h) on EliteRouteDraftLab's dataset: the
time-optimal teacher's shape variables from a canonical view of the free leg (start / goal, the ship's physical state,
the obstacles round the chord). An MLP 112 -> hidden... -> 11 (tanh), Adam, numpy only; the teacher's VALID rows only
(an infeasible scene has no shape to learn); inputs / outputs standardized, the stats saved with the weights.

usage: python tools/navigation_runtime/route_draft_train.py <out.json> <epochs> <hidden e.g. 256,256> <dataset.bin>...
"""
import json
import sys

import numpy as np

INPUTS, OUTPUTS = 112, 11
ROW = INPUTS + OUTPUTS + 2


def main():
    out_path, epochs = sys.argv[1], int(sys.argv[2])
    hidden = [int(h) for h in sys.argv[3].split(",")]
    data = np.concatenate([np.fromfile(p, dtype=np.float32).reshape(-1, ROW) for p in sys.argv[4:]])
    data = data[data[:, -1] > 0.5]
    data = data[np.isfinite(data).all(1)]
    X, Y = data[:, :INPUTS], data[:, INPUTS:INPUTS + OUTPUTS]
    # outliers (a short chord with long handles): clipped to the 1st..99th percentile per output
    lo, hi = np.percentile(Y, 1, axis=0), np.percentile(Y, 99, axis=0)
    Y = np.clip(Y, lo, hi)
    in_mean, in_std = X.mean(0), X.std(0) + 1e-3
    out_mean, out_std = Y.mean(0), Y.std(0) + 1e-4
    Xn, Yn = (X - in_mean) / in_std, (Y - out_mean) / out_std
    n = len(Xn)
    print(f"rows {n} (valid), inputs {INPUTS}, outputs {OUTPUTS}, hidden {hidden}")
    rng = np.random.default_rng(11)
    idx = rng.permutation(n)
    split = int(n * 0.9)
    tr, va = idx[:split], idx[split:]
    sizes = [INPUTS] + hidden + [OUTPUTS]
    W = [rng.normal(0, 1.0 / np.sqrt(a), (b, a)).astype(np.float32) for a, b in zip(sizes, sizes[1:])]
    B = [np.zeros(b, np.float32) for b in sizes[1:]]
    mW = [np.zeros_like(w) for w in W]; vW = [np.zeros_like(w) for w in W]
    mB = [np.zeros_like(b) for b in B]; vB = [np.zeros_like(b) for b in B]
    lr, b1, b2, eps, step = 2e-3, 0.9, 0.999, 1e-8, 0

    def forward(x):
        acts = [x]
        for i, (w, b) in enumerate(zip(W, B)):
            z = acts[-1] @ w.T + b
            acts.append(np.tanh(z) if i + 1 < len(W) else z)
        return acts

    def report(ep):
        lt = float(np.mean((forward(Xn[tr[:20000]])[-1] - Yn[tr[:20000]]) ** 2))
        pv = forward(Xn[va])[-1]
        lv = float(np.mean((pv - Yn[va]) ** 2))
        per = np.mean((pv - Yn[va]) ** 2, axis=0)
        print(f"epoch {ep + 1}: mse (standardized) train {lt:.4f} val {lv:.4f}; per output "
              + " ".join(f"{v:.2f}" for v in per))
        sys.stdout.flush()

    for ep in range(epochs):
        perm = rng.permutation(tr)
        for k in range(0, len(perm), 256):
            bi = perm[k:k + 256]
            acts = forward(Xn[bi])
            g = 2.0 * (acts[-1] - Yn[bi]) / len(bi)
            step += 1
            for i in reversed(range(len(W))):
                gW = g.T @ acts[i]
                gB = g.sum(0)
                if i > 0:
                    g = (g @ W[i]) * (1.0 - acts[i] ** 2)
                for P, G, M, V in ((W, gW, mW, vW), (B, gB, mB, vB)):
                    M[i] = b1 * M[i] + (1 - b1) * G
                    V[i] = b2 * V[i] + (1 - b2) * G * G
                    mh = M[i] / (1 - b1 ** step); vh = V[i] / (1 - b2 ** step)
                    P[i] -= lr * mh / (np.sqrt(vh) + eps)
        if ep % 5 == 4 or ep == epochs - 1:
            report(ep)
        if ep == int(epochs * 0.7):
            lr *= 0.3
    json.dump({"schema": "elite.route_draft_net.v1", "feature_version": 2, "inputs": INPUTS, "outputs": OUTPUTS, "hidden": hidden,
               "note": "tools/navigation_runtime/route_draft_train.py on EliteRouteDraftLab's time-optimal teacher",
               "in_mean": in_mean.tolist(), "in_std": in_std.tolist(),
               "out_mean": out_mean.tolist(), "out_std": out_std.tolist(),
               "layers": [{"w": w.tolist(), "b": b.tolist()} for w, b in zip(W, B)]},
              open(out_path, "w", encoding="utf-8"))
    print("written", out_path)


if __name__ == "__main__":
    main()
