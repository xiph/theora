# makefile to generate the spec document from it sources
# requires transfig and pdflatex

# Alias some shell commands, so we might have a chance of running on non-Unix
#  platforms some day.
MV = mv
RM = rm -f

SPEC_SRCS = spec.tex spec.bib

FIG_SRCS = pic-frame.fig hilbert-mb.fig hilbert-block.fig xifish.fig \
           superblock.fig macroblock.fig raster-block.fig reference-frames.fig \
           pixel444.fig pixel422.fig pixel420.fig idct.fig fdct.fig \
           pic_even.fig pic_even_odd.fig pic_odd.fig pic_odd_even.fig

FIG_TEXS = $(FIG_SRCS:.fig=.tex)
FIG_AUXS = $(FIG_SRCS:.fig=.aux)
FIG_OBJS = $(FIG_SRCS:.fig=.pdf)

#FIG_OBJS = pic-frame.pdf hilbert-mb.pdf hilbert-block.pdf xifish.pdf \
#	    superblock.pdf macroblock.odf raster-block.pdf 
#	    reference-frames.pdf \
#	    pixel444.pdf pixel422.pdf pixel420.pdf idct.pdf fdct.pdf \
#	    pic_even.tex pic_even_odd.tex pic_odd.tex pic_odd_even.tex

Theora_I_spec.pdf : spec.pdf
	$(MV) $< $@

spec.pdf : $(SPEC_SRCS) $(FIG_OBJS) vp3huff.tex spec.bbl
	# three times is the charm with references
	pdflatex --interaction nonstopmode spec.tex
	pdflatex --interaction nonstopmode spec.tex
	pdflatex --interaction nonstopmode spec.tex

spec.aux : spec.tex
	#Long tables require the .aux file to start from scratch
	-rm spec.aux
	pdflatex --interaction nonstopmode $<

spec.bbl : spec.aux spec.bib
	bibtex $<

vp3huff.tex : vp3huff
	./vp3huff > $@

figures : $(FIG_OBJS)

# rule to generate latex versions of the xfig figures
%.tex : %.fig
	fig2dev -L latex $< $@

%.pdf : %.fig
	fig2dev -L pdf -p 0 $< $@

.PHONY: clean distclean maintainer-clean

# clean targets
clean:
	-$(RM) $(FIG_TEXS)
	-$(RM) $(FIG_AUXS)
	-$(RM) vp3huff
	-$(RM) vp3huff.tex
	-$(RM) vp3huff.aux
	-$(RM) spec.aux
	-$(RM) spec.log
	-$(RM) spec.lof
	-$(RM) spec.lot
	-$(RM) spec.out
	-$(RM) spec.bbl
	-$(RM) spec.blg
	-$(RM) spec.toc

distclean: clean

maintainer-clean: distclean
	-$(RM) Theora_I_spec.pdf

maintainerclean: maintainer-clean
