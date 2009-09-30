/*
 * LNS Summation operator : log2(1+2^x)
 *
 * Author : Sylvain Collange
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or 
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  
 */

#include "LNSAdd.hpp"
#include <sstream>
#include <vector>
#include "../HOTBM.hpp"
#include "../utils.hpp"

using namespace std;

namespace flopoco{
	
	extern vector<Operator*> oplist;	// ???

	LNSAdd::LNSAdd(Target * target, int wE, int wF, int o) :
		Operator(target), wE(wE), wF(wF), order(o)
	{
		setCombinatorial();
		t[0] = t[1] = t[2] = 0;

		ostringstream name;
		name<<"LNSAdd_"<< wE <<"_"<< wF; 
		setName(name.str()); 

		setCopyrightString("Sylvain Collange (2008)");		
		setCombinatorial(); // TODO this should no longer be useful in the new framework

		addInput ("x", wE + wF);
		addOutput("r", wF, 2); // Faithful rounding;
	
		if(wF > 7) {
			// Who is going to free this memory??
			// T0 always order 1?...
			t[0] = new HOTBM(target, "log2(1+2^x)", uniqueName_, wF+wE-1, wF-7, o, -(1 << wE), -8, 1 << 7);
			oplist.push_back(t[0]);

			vhdl << tab << declare("xi0", wF+wE-1) << " <= x" << range(wF+wE-2, 0) << ";" << endl;

			inPortMap  (t[0], "x", "xi0");
			outPortMap (t[0], "r","out_t0");
			vhdl << instance(t[0], "inst_t0");

		}

		if(wF > 6) {
		
			t[1] = new HOTBM(target, "log2(1+2^x)", uniqueName_, wF+2, wF-4, o, -8, -4, 1 << 4);
			oplist.push_back(t[1]);
			vhdl << tab << declare("xi1", wF+2) << " <= x" << range(wF+1, 0) << ";" << endl;

			inPortMap  (t[1], "x", "xi1");
			outPortMap (t[1], "r","out_t1");
			vhdl << instance(t[1], "inst_t1");
		}
	
		t[2] = new HOTBM(target, "log2(1+2^x)", uniqueName_, wF+2, wF, o, -4, 0, 1);
		oplist.push_back(t[2]);

		vhdl << "  r <= ";
	
		if(wF > 7) {
			vhdl << "(" << (wF-1) << " downto " << wF-6 << " => '0') & out_t0(" << (wF-7) << " downto 0)\n"
			  << "         when x(" << (wF+wE-1) << " downto " << (wF+3) << ") /= (" << (wF+wE-1) << " downto " << (wF+3) << " => '1') else\n       ";
		}

		if(wF > 6) {
			vhdl << "(" << (wF-1) << " downto " << (wF-3) << " => '0') & out_t1(" << (wF-4) << " downto 0)\n"
			  << "         when x(" << (wF+2) << ") /= '1' else\n       ";
		}
		vhdl << "out_t2(" << (wF-1) << " downto 0);\n";

	
	}

	LNSAdd::~LNSAdd()
	{
	}



#if 0 // TODO convert to emulate() 
	void LNSAdd::fillTestCase(mpz_class a[])
	{
		/* Get inputs / outputs */
		mpz_class &x  = a[0];
		mpz_class &rd = a[1]; // rounded down
		mpz_class &ru = a[2]; // rounded up

		int outSign = 0;

		mpfr_t mpX, mpR;
		mpfr_inits(mpX, mpR, 0);

		/* Convert a random signal to an mpfr_t in [0,1[ */
		mpfr_set_z(mpX, x.get_mpz_t(), GMP_RNDN);
		mpfr_div_2si(mpX, mpX, wI, GMP_RNDN);

		/* Compute the function */
		//f.eval(mpR, mpX);

		/* Compute the signal value */
		if (mpfr_signbit(mpR))
			{
				outSign = 1;
				mpfr_abs(mpR, mpR, GMP_RNDN);
			}
		mpfr_mul_2si(mpR, mpR, wO, GMP_RNDN);

		/* NOT A TYPO. HOTBM only guarantees faithful
		 * rounding, so we will round down here,
		 * add both the upper and lower neighbor.
		 */
		mpfr_get_z(rd.get_mpz_t(), mpR, GMP_RNDD);
		ru = rd + 1;

	}

#endif
}
