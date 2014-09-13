#!/usr/bin/perl -w

# acer_5750G_fan_controller.pl
# Copyright (C) 2014  Eduard Nicodei   eddnicodei (at) gmail.com
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 3
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
#
# This software is based on acer_ec.pl version 0.6.1 (2007-11-08):
# Copyright (C) 2007  Michael Kurz     michi.kurz (at) googlemail.com
# Copyright (C) 2007  Petr Tomasek     tomasek (#) etf,cuni,cz
# Copyright (C) 2007  Carlos Corbacho  cathectic (at) gmail.com


require 5.004;

use strict;
use Fcntl;
use POSIX;
use File::Basename;

sub initialize_ioports
{
  sysopen (IOPORTS, "/dev/port", O_RDWR)
    or die "/dev/port: $!\n";
  binmode IOPORTS;
}

sub close_ioports
{
  close (IOPORTS)
    or print "Warning: $!\n";
}


# Read from a ioport.
# Takes one argument: the port number.
sub inb
{
  my ($res,$nrchars);
  sysseek IOPORTS, $_[0], 0 or return -1;
  $nrchars = sysread IOPORTS, $res, 1;
  return -1 if not defined $nrchars or $nrchars != 1;
  $res = unpack "C",$res ;
  return $res;
}

# Write to a ioport.
# $_[0]: value to write
# $_[1]: port number
# Returns: -1 on failure, 0 on success.
sub outb
{
  if ($_[0] > 0xff)
  {
    my ($package, $filename, $line, $sub) = caller(1);
    print "\n*** Called outb with value=$_[1] from line $line\n",
          "*** (in $sub). PLEASE REPORT!\n",
          "*** Terminating.\n";
    exit(-1);
  }
  my $towrite = pack "C", $_[0];
  sysseek IOPORTS, $_[1], 0 or return -1;
  my $nrchars = syswrite IOPORTS, $towrite, 1;
  return -1 if not defined $nrchars or $nrchars != 1;
  return 0;
}



# wait_until_mask_is_value(mask, expected_value) reads from port 0x6C a value,
# bitwise-ands it with the mask and compares the result
# with the expected value. This is a convoluted way of doing things.
# Originally, in acer_ec.pl, this was substituted by two functions:
# sub wait_write
# {
# 	my $i = 0;
# 	while ((inb($_[0]) & 0x02) && ($i < 10000)) {
# 		sleep(0.01);
# 		$i++;
# 	}
# 	return -($i == 10000);
# }
#
# sub wait_read
# {
# 	my $i = 0;
# 	while (!(inb($_[0]) & 0x01) && ($i < 10000)) {
# 		sleep(0.01);
# 		$i++;
# 	}
# 	return -($i == 10000);
# }
# wait_until_mask_is_value however corresponds do an equivalent function
# in eblib.dll library.

sub wait_until_mask_is_value
{
	my $i = 0;
	while ($i < 10000) {
		if ((inb(0x6C) & $_[0]) == $_[1]) {
			return 0;  # success
		}
		sleep(0.01);
		$i++;
	}
	printf("Error: timeout when waiting for mask 0x%2X\n", $_[0]);
	return -1;  # timeout
}

sub ec_intro_sequence
{
	# if the outro sequence doesn't execute, the following wait will time out,
	# (we will read back 0x80 each time)
	if (wait_until_mask_is_value(0x80, 0x00)) {
		return -1;
	}
	inb(0x68);
	# write wait.
	if (wait_until_mask_is_value(0x02, 0x00)) {
		return -1;
	}
	# magic value.
	outb(0x59, 0x6C);
}

# Probably close EC comms?
# note that this function MUST be called before exit.
# Otherwise, the first wait in ec_intro_sequence will fail.
sub ec_outro_sequence
{
	inb(0x68);
	if (wait_until_mask_is_value(0x02, 0x00)) {
		return -1;
	}
	outb(0xFF, 0x6C);
}

sub print_usage
{
	print "Usage: acer_5750_fan_control.pl MAX|NORMAL\n";
	exit -1;
}


if ($#ARGV != 0 || ($ARGV[0] ne "NORMAL" && $ARGV[0] ne "MAX")) {
	print_usage
}

initialize_ioports();

if (ec_intro_sequence()) {
	print "Unable to perform intro sequence\n";
	goto "OUTRO";
} else {
	print "ec_intro_secquence successful\n";
}

# write wait
if (wait_until_mask_is_value(0x02, 0x00)) {
	print "Error waiting for writing magic value\n";
	goto "OUTRO";
}

#default magic value is 0x77 == Turn fan to maximum speed.
my $CMD;
if ($ARGV[0] eq "MAX") {
	$CMD = 0x77;
} elsif ($ARGV[0] eq "NORMAL") {
	$CMD=0x76;
} else {
	print "BUG: we should never reach this code.";
	goto "OUTRO";
}

if (outb($CMD, 0x68)) {
	printf "Unable to write magic 0x%02X command\n", $CMD;
} else {
	printf "Successfully sent command 0x%02X\n", $CMD;
}

OUTRO:
if (ec_outro_sequence()) {
	print "Unable to perform outro sequence\n";
} else {
	print "ec_outro_sequence successful.\n";
}
close_ioports();
