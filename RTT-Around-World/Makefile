all: collect plot pdf

pdf:
	pdflatex hw7.tex
	pdflatex hw7.tex
# twice to get references right the first time

plot:
	scripts/post-process
	#scripts/ip_distance
	scripts/ip_ping_dist
	scripts/only_dist_ping
	scripts/mean_ping_dist_data
	scripts/speed_of_bits
	scripts/cdfdata
	scripts/scatterplot
	scripts/cdfplot
	scripts/speed_sd_plot

collect:
	scripts/collect

clean:
	rm -f logs/* data/raw.txt data/post-processed.txt *.aux *.pdf *.ps *.log
