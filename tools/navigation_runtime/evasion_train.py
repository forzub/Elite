"""Train the evasion net (src/game/navigation/avoidance/EvasionNet.h) on the teacher's dataset (EliteEvasionLab dataset).
A small MLP 36 -> 64 -> 64 -> 3 (tanh), Adam, MSE on the canonical acceleration / mainAccel; numpy only.

usage: python tools/navigation_runtime/evasion_train.py <dataset.csv> <out.json> [epochs]
"""
import json
import sys

import numpy as np


def main():
    data = np.loadtxt(sys.argv[1], delimiter=",", dtype=np.float32)
    epochs = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    X, Y = data[:, :36], data[:, 36:39]
    n = len(X)
    rng = np.random.default_rng(7)
    idx = rng.permutation(n)
    split = int(n * 0.9)
    tr, va = idx[:split], idx[split:]
    sizes = [36, 64, 64, 3]
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

    for ep in range(epochs):
        perm = rng.permutation(tr)
        for k in range(0, len(perm), 256):
            bi = perm[k:k + 256]
            acts = forward(X[bi])
            g = 2.0 * (acts[-1] - Y[bi]) / len(bi)
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
            lt = float(np.mean((forward(X[tr[:20000]])[-1] - Y[tr[:20000]]) ** 2))
            lv = float(np.mean((forward(X[va])[-1] - Y[va]) ** 2))
            print(f"epoch {ep + 1}: mse train {lt:.4f} val {lv:.4f} (target variance {float(np.var(Y[va])):.4f})")
        if ep == int(epochs * 0.7):
            lr *= 0.3
    json.dump({"schema": "elite.evasion_net.v1", "inputs": 36,
               "note": "tools/navigation_runtime/evasion_train.py on EliteEvasionLab's teacher dataset",
               "layers": [{"w": w.tolist(), "b": b.tolist()} for w, b in zip(W, B)]},
              open(sys.argv[2], "w", encoding="utf-8"))
    print("written", sys.argv[2])


if __name__ == "__main__":
    main()
