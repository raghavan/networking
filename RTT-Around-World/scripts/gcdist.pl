# To calculate the distance between London (51.3N 0.5W) 
# and Tokyo (35.7N 139.8E) in kilometers: 

use Math::Trig qw(great_circle_distance deg2rad);

# Notice the 90 - latitude: phi zero is at the North Pole.
@L = (deg2rad($ARGV[1]), deg2rad(90-$ARGV[0]));
@T = (deg2rad($ARGV[3]),deg2rad(90 - $ARGV[2]));

$km = great_circle_distance(@L, @T, 6378);
printf "%.1f\n",$km
