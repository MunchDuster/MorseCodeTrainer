import requests

URL = 'https://app.aspell.net/create?max_size=35&spelling=GBs&variant_level=0&diacritic=strip&download=wordlist&encoding=utf-8&format=inline'
OUTPUT_FILE = "data/words_2-letters.txt"

def is_valid(word):
	return len(word) == 2 and "'" not in word

def main():
	print("Downloading word list...")
	response = requests.get(URL)
	response.raise_for_status()
	
	print("Processing...")
	text = response.text.split('---')[1]
	words = text.splitlines()
	print("Total:", len(words))

	print("Sorting and filtering...")
	filtered = sorted(
		{
			w.strip().lower()
			for w in words
			if is_valid(w.strip())
		}
	)

	print("Filtered:", len(filtered))

	with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
		f.write("\n".join(filtered))

	print("Saved:", OUTPUT_FILE)

if __name__ == "__main__":
	main()