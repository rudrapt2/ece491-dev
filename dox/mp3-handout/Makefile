TARGET=mp3
RUBRIC=mp3-rubric

pdf:
	pdflatex -shell-escape $(TARGET).tex
	
all: $(TARGET).pdf

$(RUBRIC).pdf: $(RUBRIC).ps
	ps2pdf $<

$(RUBRIC).ps: $(RUBRIC).dvi
	dvips -t letter -o $@ $<

$(RUBRIC).dvi: $(RUBRIC).tex
	latex $<
	latex $<

$(TARGET).pdf: $(TARGET).ps
	ps2pdf $<

$(TARGET).ps: $(TARGET).dvi
	dvips -t letter -o $@ $<

$(TARGET).dvi: $(TARGET).tex
	latex $<
	latex $<

clean::
	-rm -f $(TARGET).dvi $(TARGET).ps *.log *.aux *.toc *.out $(RUBRIC).ps $(RUBRIC).dvi

clear: clean
	-rm -f $(TARGET).pdf $(RUBRIC).pdf
