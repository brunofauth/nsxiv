data.gen.h: img2data
	find -type f -iname '*.png' -printf '%P\n' | sort -g | xargs ./img2data > $@

img2data: img2data.c
	gcc -Wall -std=c89 -pedantic -lImlib2 img2data.c -o img2data

.PHONY: clean
clean:
	rm ./img2data ./data.gen.h

