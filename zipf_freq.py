from collections import Counter

freq = Counter()

with open("stems.txt", "r", encoding="utf-8") as f:
    for line in f:
        w = line.strip()
        if w:
            freq[w] += 1

with open("freq.tsv", "w", encoding="utf-8") as out:
    out.write("rank\tword\tfreq\n")
    for rank, (word, count) in enumerate(freq.most_common(), start=1):
        out.write(f"{rank}\t{word}\t{count}\n")
