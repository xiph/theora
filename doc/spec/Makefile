# makefile to generate the spec document from it sources
# requires transfig and pdflatex

# Alias some shell commands, so we might have a chance of running on non-Unix
#  platforms some day.
MV = mv
RM = rm -f

SPEC_SRCS = spec.tex spec.bib

FIG_SRCS = pic-frame.fig hilbert-mb.fig hilbert-block.fig xifish.fig

FIG_TEXS = $(FIG_SRCS:.fig=.tex)
FIG_PDFS = $(FIG_SRCS:.fig=.pdf)

FIG_OBJS = pic-frame.tex hilbert-mb.tex hilbert-block.tex xifish.pdf

Theora_I_spec.pdf : spec.pdf
	$(MV) $< $@

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

%.pdf : %.fig
	fig2dev -L pdf $< $@

.PHONY: clean distclean maintainer-clean

# clean targets
clean:
	-$(RM) $(FIG_OBJS)
	-$(RM) spec.pdf
	-$(RM) spec.aux
	-$(RM) spec.log
	-$(RM) spec.out
	-$(RM) spec.bbl
	-$(RM) spec.blg
	-$(RM) spec.toc

distclean: clean

maintainer-clean: distclean
	-$(RM) Theora_I_spec.pdf

maintainerclean: maintainer-clean
