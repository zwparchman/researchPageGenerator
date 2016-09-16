DOC=paper
$(DOC).pdf : $(DOC).tex  *.tex bibfile
	pdflatex $(DOC)
	bibtex  $(DOC)
	pdflatex $(DOC)
	pdflatex $(DOC)


bibfile: 
	-@cd ./bibs > /dev/null ; ./cat-bibfiles.sh > /dev/null ; cd ..

clean:
	rm -f $(DOC).pdf
	rm -f *aux
	rm -f $(DOC).out
	rm -f $(DOC).log
	rm -f *pyc
