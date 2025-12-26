import matplotlib.pyplot as plt

ranks = []
freqs = []

with open("freq.tsv", "r", encoding="utf-8") as f:
    next(f)  # skip header
    for line in f:
        r, _, fr = line.strip().split("\t")
        ranks.append(int(r))
        freqs.append(int(fr))

plt.figure(figsize=(7, 5))
plt.loglog(ranks, freqs)
plt.xlabel("Rank")
plt.ylabel("Frequency")
plt.title("Zipf's law for video game corpus")
plt.grid(True)

plt.savefig("zipf.png", dpi=150)
plt.show()
