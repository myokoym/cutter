# -*- rd -*-

= Install to Ubuntu --- How to install Cutter to Ubuntu

== Introduction

This document explains how to install Cutter to Ubuntu
Linux.

== Install

We can use aptitude because Cutter provides Debian packages.

There are packages for Ubuntu Precise Pangolin/Saucy Salamander/Trusty Tahr i386/amd64.
If you need a package for another environment,
you can request on ((<mailing
list|URL:https://lists.sourceforge.net/lists/listinfo/cutter-users-en>)).

=== PPA (Personal Package Archive)

The Cutter APT repository for Ubuntu uses PPA (Personal Package Archive) on Launchpad since Cutter 1.2.4. You can install Cutter by APT from the PPA.

Here are supported Ubuntu versions:

  * 12.04 LTS Precise Pangolin
  * 13.10 Saucy Salamander
  * 14.04 LTS Trusty Tahr

Enable the universe repository to install Cutter:

  % sudo apt-get -y install software-properties-common
  % sudo add-apt-repository -y universe

Add the (({ppa:cutter-testing-framework/ppa})) PPA to your system:

  % sudo add-apt-repository -y ppa:cutter-testing-framework/ppa
  % sudo apt-get update

Install:

  % sudo apt-get -y install cutter-testing-framework

== The next step

Installation is completed. We should try
((<tutorial|TUTORIAL>)) with the installed Cutter.
