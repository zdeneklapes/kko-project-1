# makefile pro preklad LaTeX verze Bc. prace
# makefile for compilation of the thesis
# (c) 2008 Michal Bidlo
# E-mail: bidlom AT fit vut cz
# Edited by: dytrych AT fit vut cz
#===========================================
# asi budete chtit prejmenovat / you will probably rename:
CO=xlapes02
CMD_PDFLATEX=pdflatex
CMD_BIBTEX=bibtex
OUT_DIR=./out
FLAGS=-synctex=1 -output-directory=$(OUT_DIR)
RM=rm -rfd

all: $(CO).pdf

pdf: $(CO).pdf

html: $(CO).tex
	pandoc --toc xlapes02.tex -s --mathjax -o $(OUT_DIR)/xlapes02.html --metadata title="Zdenek Lapes"

$(CO).ps: $(CO).dvi
	dvips $(CO)

$(CO).pdf: clean setup
	$(CMD_PDFLATEX) $(FLAGS) $(CO)
	-$(CMD_BIBTEX) $(OUT_DIR)/$(CO)
	$(CMD_PDFLATEX) $(FLAGS) $(CO)
	$(CMD_PDFLATEX) $(FLAGS) $(CO)
	$(CMD_PDFLATEX) $(FLAGS) $(CO)

open:
	open -a Skim $(OUT_DIR)/$(CO).pdf

$(CO).dvi: $(CO).tex $(CO).bib
	latex $(CO)
	-bibtex $(CO)
	latex $(CO)
	latex $(CO)
	latex $(CO)

clean:
	$(RM) $(OUT_DIR)
	find . -iname "*.log" -delete

setup:
	mkdir $(OUT_DIR)

pack:
	tar czvf $(CO).tar.gz *.tex *.bib *.bst ./template-fig/* ./bib-styles/* ./cls/* zadani.pdf $(CO).pdf Makefile Changelog

rename:
	mv $(CO).tex $(NAME).tex
	mv $(CO)-01-kapitoly-chapters.tex $(NAME)-01-kapitoly-chapters.tex
	mv $(CO)-01-kapitoly-chapters-en.tex $(NAME)-01-kapitoly-chapters-en.tex
	mv $(CO)-20-literatura-bibliography.bib $(NAME)-20-literatura-bibliography.bib
	mv $(CO)-30-prilohy-appendices.tex $(NAME)-30-prilohy-appendices.tex
	mv $(CO)-30-prilohy-appendices-en.tex $(NAME)-30-prilohy-appendices-en.tex
	sed -i "s/$(CO)-01-kapitoly-chapters/$(NAME)-01-kapitoly-chapters/g" $(NAME).tex
	sed -i "s/$(CO)-01-kapitoly-chapters-en/$(NAME)-01-kapitoly-chapters-en/g" $(NAME).tex
	sed -i "s/$(CO)-20-literatura-bibliography/$(NAME)-20-literatura-bibliography/g" $(NAME).tex
	sed -i "s/$(CO)-30-prilohy-appendices/$(NAME)-30-prilohy-appendices/g" $(NAME).tex
	sed -i "s/$(CO)-30-prilohy-appendices-en/$(NAME)-30-prilohy-appendices-en/g" $(NAME).tex
	sed -i "s/$(CO)/$(NAME)/g" Makefile
	
# Pozor, vlna neresi vse (viz popis.txt) / Warning - vlna is not solving all problems (see description.txt)
vlna:
	vlna -l $(CO)-*.tex

# Spocita normostrany / Count of standard pages
normostrany:
	echo "scale=2; `detex -n $(CO)-[01]*.tex | sed s/"^ *"/""/ | sed s/"^	*"/""/ | wc -c`/1800;" | bc
