/*
 * MPEG Layer3 header info extraction routines
 *
 * Author: Erik Gustavsson, <cyrano@algonet.se>
 *
 * This software is released under the GNU General Public License.
 * Please read the included file COPYING for more information.
 * This software comes with no warranty of any kind, use at you own risk!
 */


int get_bitrate(unsigned char *buf,int bufsize);
int get_samprate(unsigned char *buf,int bufsize);
int get_mpgver(unsigned char *buf,int bufsize);
int get_stereo(unsigned char *buf,int bufsize);
