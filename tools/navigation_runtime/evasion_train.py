"""Train the evasion net (src/game/navigation/avoidance/EvasionNet.h) on the teacher's dataset (EliteEvasionLab dataset).
An MLP 36 -> hidden... -> out (tanh), Adam; numpy only.
  reg: 3 outputs, MSE on the canonical acceleration / mainAccel (averages equally good dodges - the weak variant)
  cls: 289 outputs (the teacher's manoeuvre set in the canonical frame), softmax cross-entropy on the teacher's class
  risk: 289 outputs trained on the OUTCOME of every manoeuvre (EliteEvasionLab costs -> .bin): the loss is the expected
        cost of the net's choice - a big penalty for a collision (margin < 0) plus its depth, a small one for thrust;
        no imitation, so the equally good dodges both stay good and the bad middle is punished (author 2026-11-23:
        «я бы штрафовал за столкновение»)

usage: python tools/navigation_runtime/evasion_train.py <dataset.csv> <out.json> [epochs] [reg|cls] [hidden e.g. 128,128]
"""
import json
import sys

import numpy as np

CLASSES = 96 * 3 + 1


def main():
    epochs = int(sys.argv[3]) if len(sys.argv) > 3 else 40
    mode = sys.argv[4] if len(sys.argv) > 4 else "reg"
    hidden = [int(h) for h in sys.argv[5].split(",")] if len(sys.argv) > 5 else [64, 64]
    if mode == "risk":
        data = np.fromfile(sys.argv[1], dtype=np.float32).reshape(-1, 36 + 2 * CLASSES)
    else:
        data = np.loadtxt(sys.argv[1], delimiter=",", dtype=np.float32)
    X = data[:, :36]
    if mode == "risk":
        margin = data[:, 36:36 + CLASSES]
        score = data[:, 36 + CLASSES:]
        # the cost of each manoeuvre: a collision 1.0 + its depth (1 per 100 m), then a little for the margin short of
        # 200 m and the thrust (the score's tail) - all positive, the safe ones near 0
        Y = (margin < 0).astype(np.float32) + np.clip(-margin, 0, 300) / 100.0 \
            + 0.05 * (200.0 - np.clip(score, -200, 200)) / 400.0
        Y = Y.astype(np.float32)
        out_n = CLASSES
    elif mode == "cls":
        labels = data[:, 40].astype(np.int64)
        Y = np.zeros((len(X), CLASSES), np.float32)
        Y[np.arange(len(X)), labels] = 1.0
        out_n = CLASSES
    else:
        Y = data[:, 36:39]
        out_n = 3
    n = len(X)
    rng = np.random.default_rng(7)
    idx = rng.permutation(n)
    split = int(n * 0.9)
    tr, va = idx[:split], idx[split:]
    sizes = [36] + hidden + [out_n]
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

    def softmax(z):
        z = z - z.max(1, keepdims=True)
        e = np.exp(z)
        return e / e.sum(1, keepdims=True)

    def report(ep):
        if mode == "risk":
            for name, s in (("train", tr[:20000]), ("val", va)):
                z = forward(X[s])[-1]
                pick = z.argmax(1)
                m = margin[s][np.arange(len(s)), pick]
                any_safe = (margin[s] >= 0).any(1)
                top12 = np.argsort(-z, 1)[:, :12]
                safe12 = (np.take_along_axis(margin[s], top12, 1) >= 0).any(1)
                exp_cost = float(np.mean((softmax(z) * Y[s]).sum(1)))
                print(f"epoch {ep + 1}: {name} expected cost {exp_cost:.3f}; top1 safe {float(np.mean(m >= 0)):.3f}, "
                      f"a safe one in top12 {float(np.mean(safe12)):.3f}, any safe exists {float(np.mean(any_safe)):.3f}")
            sys.stdout.flush()
            return
        if mode == "cls":
            for name, s in (("train", tr[:20000]), ("val", va)):
                z = forward(X[s])[-1]
                lab = Y[s].argmax(1)
                top1 = float(np.mean(z.argmax(1) == lab))
                top12 = float(np.mean((np.argsort(-z, 1)[:, :12] == lab[:, None]).any(1)))
                print(f"epoch {ep + 1}: {name} top1 {top1:.3f} top12 {top12:.3f}")
        else:
            lt = float(np.mean((forward(X[tr[:20000]])[-1] - Y[tr[:20000]]) ** 2))
            lv = float(np.mean((forward(X[va])[-1] - Y[va]) ** 2))
            print(f"epoch {ep + 1}: mse train {lt:.4f} val {lv:.4f} (target variance {float(np.var(Y[va])):.4f})")
        sys.stdout.flush()

    for ep in range(epochs):
        perm = rng.permutation(tr)
        for k in range(0, len(perm), 256):
            bi = perm[k:k + 256]
            acts = forward(X[bi])
            if mode == "risk":
                p = softmax(acts[-1])
                c = Y[bi]
                # d/dz of sum p c = p (c - sum p c)
                g = p * (c - (p * c).sum(1, keepdims=True)) / len(bi)
            elif mode == "cls":
                g = (softmax(acts[-1]) - Y[bi]) / len(bi)
            else:
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
            report(ep)
        if ep == int(epochs * 0.7):
            lr *= 0.3
    json.dump({"schema": "elite.evasion_net.v1", "inputs": 36, "mode": mode, "hidden": hidden,
               "note": "tools/navigation_runtime/evasion_train.py on EliteEvasionLab's teacher dataset",
               "layers": [{"w": w.tolist(), "b": b.tolist()} for w, b in zip(W, B)]},
              open(sys.argv[2], "w", encoding="utf-8"))
    print("written", sys.argv[2])


if __name__ == "__main__":
    main()
