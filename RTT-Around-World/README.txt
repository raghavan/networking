Using Planet-lab machines to find the RTT between each country and try to locate an approximate location of a IP based on its ping time.

Using the latex template and gnuplot, create a report containing the following elements. Each element is to be duplicated for at least two independent sets of measurements, at significantly different times of day. Report the time and date for each set.
Scatterplot of pair-wise round-trip time measurements. Plot the distance between the hosts on the X axis, and the round-trip time on the Y axis. For example, "plot [:] [0:500] 'thefile' using 1:2" would generate a scatterplot.

Cumulative distribution function (CDF) plot for round-trip time, and for distance. The y-axis of a CDF goes from 0 to 1. It is zero for the minimum X value, and 1 for the maximum X value, and grows with X. Using a CDF, one can quickly look up the median value (where y=0.5), or any percentile.
Compute the mean and standard deviation of 'speed of bits' over each set. Was there a significant difference, if so, what may be the cause of it?

Complete the following analyzes using all your measurements:
estimate the maximum (uncongested) 'speed of bits' on the Internet, in km/s. How does it compare to the speed of light, and why are they different?
some planetlab sites have misreported their locations. List a few obvious examples, and explain your reasoning.
do any particular sites, or perhaps countries, have particularly poor connectivity? show an example, and explain your reasoning.
Finally, estimate the location (try to get country and city, or if not, give your best lat/lon guess) of IP address 68.86.95.9 using the same type of ping measurements from planetlab hosts. Present the evidence you used to arrive at your conclusion, and describe your reasoning.
