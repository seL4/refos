#
# Copyright 2014, NICTA
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(NICTA_BSD)
#

# $Id: Makefile,v 1.26 2009-04-21 05:40:29 gernot Exp $
#
#  To create a LaTeX diff against the CVS HEAD revision, use the target
#  "diff" (or "viewdiff").
#  To use a revision other than HEAD, speciff DIFF=x on the 
#  make command line to choose the revision x.
#

.PHONY: all

BIBDIR     ?= /home/disy/lib/BibTeX:../../../bibtex
LaTeXEnv   = TEXINPUTS=".:/home/disy/lib/TeX:/home/disy/lib/ps:${TEXINPUTS}:"
BibTexEnv  = BIBINPUTS=".:${BIBDIR}:${BIBINPUTS}:"

LaTeX      = ${LaTeXEnv} pdflatex -shell-escape -interaction=nonstopmode
BibTeX     = ${BibTexEnv} bibtex
Fig2Eps    = fig2dev -L eps
Dia2Eps    = dia -t eps -e
GnuPlot	   = gnuplot
Eps2Pdf	   = epstopdf --outfile
PdfView	   = xpdf
#PdfView    = open -a preview
#PdfView    = acroread 
Lpr 	   = lpr
mv	   = mv
awk	   = awk
R	   = R

Targets    = paper


# CONFIGURATION OPTIONS
# =====================

# Extra figures that aren't supplied as dia, gnuplot or fig sources
# (eg figures already supplied in PDF, or supplied in EPS).
# List with PDF extensions:
ExtraFigs= #imgs/eps-fig.pdf

# Any other stuff that may be needed

# END CONFIGURATION OPTIONS
# =========================

Optional = $(addsuffix -diff, $(Targets))
All = $(Targets) $(Optional)

Perf_Sources = $(wildcard imgs/*.perf)
Dia_Sources = $(wildcard imgs/*.dia)
Fig_Sources = $(wildcard imgs/*.fig)
Gnuplot_Sources = $(wildcard imgs/*.gnuplot)
R_Sources = $(wildcard imgs/*.r)
Figures = $(Perf_Sources:.perf=.pdf) $(Dia_Sources:.dia=.pdf) $(Fig_Sources:.fig=.pdf) $(Gnuplot_Sources:.gnuplot=.pdf)  $(R_Sources:.r=.pdf) $(ExtraFigs)

Aux = $(addsuffix .aux, $(Targets))
Bib = extra.bib
Pdf = $(addsuffix .pdf, $(Targets))
Diff_Pdf = $(addsuffix .pdf, $(Optional))
Inputs = $(wildcard *.tex)

.PHONY: FORCE

all: pdf
diff: diff_pdf
FORCE: pdf
ps: $(Ps)
pdf: $(Bib) $(Figures) Makefile $(Pdf) 
diff_pdf: $(Bib) $(Figures) Makefile $(Diff_Pdf)


%.pdf: %.perf tools/bargraph.pl
	@echo $< '->' $@
	@ ${BarGraph} -pdf $< > $@

%.pdf: %.eps
	@echo $< '->' $@
	@ ${Eps2Pdf} $@ $<

%.pdf: %.ps
	@echo $< '->' $@
	@ ${Eps2Pdf} $@ $<

%.eps: %.dia
	@echo $< '->' $@
	@ ${Dia2Eps} $@ $<

%.eps: %.fig
	@echo $< '->' $@
	@ ${Fig2Eps} $< $@

%.eps: %.gnuplot
	@echo $< '->' $@
	@ ${GnuPlot} $<

%.eps: %.r
	@echo $< '->' $@
	@ ${R} --vanilla < $<

view: pdf
	${PdfView} $(Pdf) &

viewdiff: diff
	${PdfView} $(Diff_Pdf) &

print: pdf
	${Lpr} $(Pdf)

clean:
	rm -f *.aux *.toc *.bbl *.blg *.dvi *.log *.pstex* *.eps *.cb *.brf \
		*.out *.ps *-diff.tex *.mps .log msc/* paper.pdf

realclean: clean
	rm -f *~ *.pdf *.tgz

tar:	realclean
	( p=`pwd` && d=`basename "$$p"` && cd .. && \
	  tar cfz $$d.tgz $$d && \
	  mv $$d.tgz $$d )

help:
	@echo "Main targets: all view print clean realclean tar"

##############################################################################

Rerun = '(There were undefined references|Rerun to get (cross-references|the bars) right)'
Rerun_Bib = 'No file.*\.bbl|Citation.*undefined'
Undefined = '((Reference|Citation).*undefined)|(Label.*multiply defined)'
Error = '^! '
%.pdf: %.tex $(Bib) $(Figures) Makefile $(Inputs)
	@if ! test -e $*.bbl || test $(Bib) -nt $*.bbl; then rm -f $*.bbl; fi
	@mkdir -p msc
	@echo "====> LaTeX first pass"
	@which mscgen >/dev/null 2>/dev/null || { echo "mscgen not installed" >&2 ; exit 1 ; }
	@$(LaTeX) $< >.log || if egrep -q $(Error) $*.log ; then cat .log; rm $@; false ; fi
	@if egrep -q $(Rerun_Bib) $*.log ; then echo "====> BibTex" && $(BibTeX) $* && echo "====> LaTeX BibTeX pass" && $(LaTeX) >.log $< ; fi
	@if egrep -q $(Rerun) $*.log ; then echo "====> LaTeX rerun" && $(LaTeX) >.log $<; fi
	@if egrep -q $(Rerun) $*.log ; then echo "====> LaTeX rerun" && $(LaTeX) >.log $<; fi
	@if egrep -q $(Rerun) $*.log ; then echo "====> LaTeX rerun" && $(LaTeX) >.log $<; fi
	@echo "====> Undefined references and citations:"
	@egrep -i $(Undefined) $*.log || echo "None."
	@echo "====> Dimensions:"
	@grep "dimension:" $*.log || echo "None."

##############################################################################
# Generate a list of FIXMEs 
fixmes: 
	@nl -b a ${Targets}.tex | grep "FIXME{" | nl -b a
	@echo -n "Total FIXMES: " && grep "FIXME{" ${Targets}.tex | wc -l
