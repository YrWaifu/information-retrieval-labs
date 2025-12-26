# Information Retrieval Labs (Wikipedia: Video games)

Проект для лабораторных по дисциплине «Информационный поиск».  
Корпус: статьи англоязычной Wikipedia по тематике видеоигр (категория `Category:Video games`), 30 000 документов.

---

## Структура проекта

- `download_wiki_corpus*.py` — загрузка корпуса из Wikipedia API (с возобновлением).
- `tokenize.cpp` — токенизация документов.
- `stem.cpp` — стемминг токенов.
- `build_index.cpp` — построение булевого инвертированного индекса (`dict.tsv`, `postings.bin`, `maxdoc.txt`).
- `boolean_search.cpp` — булев поиск по индексу (AND/OR/NOT, скобки).

---

## Требования

- Linux/WSL
- `g++` с поддержкой C++17
- `python3` (для загрузчика и построения графиков)
- `pip3 install matplotlib` для графиков

---

## 0) Сборка (из корня проекта)

```bash
g++ -O2 -std=c++17 tokenize.cpp -o tokenize
g++ -O2 -std=c++17 stem.cpp -o stem
g++ -O2 -std=c++17 build_index.cpp -o build_index
g++ -O2 -std=c++17 boolean_search.cpp -o boolean_search
```

## 1) Сбор корпуса (если корпуса ещё нет)

```bash
python3 download_wiki_corpus_en.py
# результат: corpus/*.txt и meta.tsv
```

Проверка количества документов:
```bash
ls corpus/*.txt | wc -l
```

## 2) Токенизация → стемминг

```bash
rm -rf tokens stems
mkdir -p tokens stems

./tokenize --dir corpus --out tokens

for f in tokens/*.tok; do
  ./stem < "$f" > "stems/$(basename "$f" .tok).stm"
done
```

## 3) Построение индекса

```bash
rm -rf index
mkdir -p index

./build_index --stems stems --out index
# результат: index/dict.tsv, index/postings.bin, index/maxdoc.txt
```

## 4) Запуск булевого поиска

```bash
./boolean_search --dict index/dict.tsv --postings index/postings.bin --maxdoc index/maxdoc.txt
```

Примеры запросов:
```text
nintendo
10-year AND NOT 10-year-old
10-year-old OR 10-year
(10-minute OR 10-yard) AND 10-year
```