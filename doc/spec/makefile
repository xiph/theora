# makefile to generate the spec document from it sources

SPEC_SRCS = spec.tex spec.bib

FIG_SRCS = hilbert-block.fig hilbert-mb.fig

FIG_OBJS = $(FIG_SRCS:.fig=.tex)

Theora_I_spec.pdf : spec.pdf
	mv $< $@

spec.pdf : $(SPEC_SRCS) $(FIG_OBJS) spec.bbl
	# three times is the charm with references
	pdflatex --interaction nonstopmode spec.tex
	pdflatex --interaction nonstopmode spec.tex
	pdflatex --interaction nonstopmode spec.tex

spec.aux : spec.tex
	pdflatex --interaction nonstopmode $<

spec.bbl : spec.aux spec.bib
	bibtex $<

figures : $(FIG_OBJS)

# rule to generate latex versions of the xfig figures
%.tex : %.fig
	fig2dev -L latex $< $@


.PHONY: clean distclean maintainer-clean

# clean targets
clean:
	-rm -f $(FIG_OBJS)
	-rm -f spec.pdf
	-rm -f spec.aux
	-rm -f spec.log
	-rm -f spec.out
	-rm -f spec.bbl
	-rm -f spec.blg
	-rm -f spec.toc

distclean: clean

maintainer-clean: distclean
	-rm -f Theora_I_spec.pdf

maintainerclean: maintainer-clean
