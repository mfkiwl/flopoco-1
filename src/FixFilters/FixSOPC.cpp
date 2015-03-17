#include <iostream>
#include <sstream>
#include "gmp.h"
#include "mpfr.h"

#include "sollya.h"

#include "FixSOPC.hpp"

#include "ConstMult/FixRealKCM.hpp"

using namespace std;
namespace flopoco{

	const int veryLargePrec = 32*20000;  /*640 Kbits should be enough for anybody */


	FixSOPC::FixSOPC(Target* target_, int lsbIn_, int lsbOut_, vector<string> coeff_) : 
		Operator(target_),  lsbOut(lsbOut_), coeff(coeff_), g(-1), computeGuardBits(true), addFinalRoundBit(true), computeMSBOut(true)
	{
		n = coeff.size();
		for (int i=0; i<n; i++) {
			msbIn.push_back(0);
			lsbIn.push_back(lsbIn_);
		}
		initialize();
	}
	

	FixSOPC::FixSOPC(Target* target_, vector<int> msbIn_, vector<int> lsbIn_, int msbOut_, int lsbOut_, vector<string> coeff_, int g_) :
		Operator(target_),  msbIn(msbIn_), lsbIn(lsbIn_), msbOut(msbOut_), lsbOut(lsbOut_), coeff(coeff_), g(g_), computeMSBOut(false)
	{
		n = coeff.size();
		if (g==-1)
			computeGuardBits=true;

		addFinalRoundBit = (g==0 ? false : true);

		initialize();
	}




	FixSOPC::~FixSOPC()
	{
		for (int i=0; i<n; i++) {
			mpfr_clear(mpcoeff[i]);
		}
	}





	void FixSOPC::initialize()	{
		srcFileName="FixSOPC";
					
		ostringstream name;
		name<<"FixSOPC_uid"<<getNewUId(); 
		setName(name.str()); 
	
		setCopyrightString("Matei Istoan, Louis Besème, Florent de Dinechin (2013-2015)");
		
		for (int i=0; i< n; i++)
			addFixInput(join("X",i), true, msbIn[i], lsbIn[i]); 

		//reporting on the filter
		ostringstream clist;
		for (int i=0; i< n; i++)
			clist << "    " << coeff[i] << ", ";
		REPORT(INFO, "Building a " << n << "-tap FIR; lsbOut=" << lsbOut << " for coefficients " << clist.str());


		if(computeGuardBits) {
			// guard bits for a faithful result
			g = 1+ intlog2(n-1); 
			REPORT(INFO, "g=" << g);
		}

		
		for (int i=0; i< n; i++)	{
				// parse the coeffs from the string, with Sollya parsing
				sollya_obj_t node;
				
				node = sollya_lib_parse_string(coeff[i].c_str());
				// If conversion did not succeed (i.e. parse error)
				if(node == 0)	{
						ostringstream error;
						error << srcFileName << ": Unable to parse string " << coeff[i] << " as a numeric constant" << endl;
						throw error.str();
					}
				
				mpfr_init2(mpcoeff[i], veryLargePrec);
				sollya_lib_get_constant(mpcoeff[i], node);
				sollya_lib_clear_obj(node);
			}
		

		if(computeMSBOut) {
			mpfr_t sumAbsCoeff, absCoeff;
			mpfr_init2 (sumAbsCoeff, veryLargePrec);
			mpfr_init2 (absCoeff, veryLargePrec);
			mpfr_set_d (sumAbsCoeff, 0.0, GMP_RNDN);
			for (int i=0; i< n; i++)	{
			// Accumulate the absolute values
				mpfr_abs(absCoeff, mpcoeff[i], GMP_RNDU);
				mpfr_add(sumAbsCoeff, sumAbsCoeff, mpcoeff[i], GMP_RNDU);
			}

			// now sumAbsCoeff is the max value that the filter can take.
			double sumAbs = mpfr_get_d(sumAbsCoeff, GMP_RNDU); // just to make the following loop easier
			REPORT(INFO, "sumAbs=" << sumAbs);
			msbOut=1;
			while(sumAbs>=2.0)		{
					sumAbs*=0.5;
					msbOut++;
				}
			while(sumAbs<1.0)	{
					sumAbs*=2.0;
					msbOut--;
				}
			REPORT(INFO, "Worst-case weight of MSB of the result is " << msbOut);
			mpfr_clears(sumAbsCoeff, absCoeff, NULL);
		}			

		addFixOutput("R", true, msbOut, lsbOut); 

#if 0
		wO = 1+ (leadingBit - (-p)) + 1; //1 + sign  ; 



#if 0		
		ostringstream dlist;
		for (int i=0; i< n; i++) {
			char *ptr;
			mp_exp_t exp;
			ptr = mpfr_get_str (0,  &exp, 10, 0, mpcoeff[i], GMP_RNDN);
			dlist << "  ." << ptr  << "e"<< exp <<  ", ";
			if(exp>0)
				THROWERROR("Coefficient #" << i << " ("<< coeff[i]<<") larger than one in absolute value")
			mpfr_free_str(ptr);
		}
		REPORT(DEBUG, "After conversion to MPFR: " << dlist.str());
#endif		


		int size = 1+ (leadingBit - (-p) +1) + g ; // sign + overflow  bits on the left, guard bits on the right
		REPORT(INFO, "Sum size is: "<< size );
		
		//compute the guard bits from the KCM mulipliers
		int guardBitsKCM = 0;
		int wInKCM = p + 1;	//p bits + 1 sign bit
		int lsbOutKCM = -p-g;
		
		for(int i=0; i<n; i++)
		{
			double targetUlpError = 1.0;
			int temp = FixRealKCM::neededGuardBits(target, wInKCM, lsbOutKCM, targetUlpError);
			
			if(temp > guardBitsKCM)
				guardBitsKCM = temp;
		}
		
		size += guardBitsKCM; // sign + overflow  bits on the left, guard bits + guard bits from KCMs on the right
		REPORT(INFO, "Sum size with KCM guard bits is: "<< size);
		
		if(!target->plainVHDL())
		{
			//create the bitheap that computes the sum
			bitHeap = new BitHeap(this, size);
			
			for (int i=0; i<n; i++) 
			{
				// Multiplication: instantiating a KCM object. It will add bits also to the right of lsbOutKCM
				FixRealKCM* mult = new FixRealKCM(this,				// the envelopping operator
														target, 	// the target FPGA
														getSignalByName(join("X",i)),
														true, 		// signed
														-1, 		// input MSB, but one sign bit will be added
														-p, 		// input LSB weight
														lsbOutKCM, 		// output LSB weight -- the output MSB is computed out of the constant
														coeff[i], 	// pass the string unmodified
														bitHeap		// pass the reference to the bitheap that will accumulate the intermediary products
													);
			}
			
			//rounding - add 1/2 ulps
			bitHeap->addConstantOneBit(g+guardBitsKCM-1);
			
			//compress the bitheap
			bitHeap -> generateCompressorVHDL();
			
			vhdl << tab << "R" << " <= " << bitHeap-> getSumName() << range(size-1, g+guardBitsKCM) << ";" << endl;
		}

		else // plainVHDL currently doesn't work because of FixRealKCM. 

		{
			// All the KCMs in parallel
			for(int i=0; i< n; i++)	{
				FixRealKCM* mult = new FixRealKCM(target, 
																					true, // signed
																					-1, // input MSB, but one sign bit will be added
																					-p, // input LSB weight
																					-p-g, // output LSB weight -- the output MSB is computed out of the constant
																					coeff[i] // pass the string unmodified
																					);
				addSubComponent(mult);
				inPortMap(mult,"X", join("X", i));
				outPortMap(mult, "R", join("P", i));
				vhdl << instance(mult, join("mult", i));
			}
			// Now advance to their output, and build a pipelined rake
			syncCycleFromSignal("P0");
			vhdl << tab << declare("S0", size) << " <= " << zg(size) << ";" << endl;
			for(int i=0; i< n; i++)		{				
				//manage the critical path
				manageCriticalPath(target->adderDelay(size));
				// Addition
				int pSize = getSignalByName(join("P", i))->width();
				vhdl << tab << declare(join("S", i+1), size) << " <= " <<  join("S",i);
#if 0 // Since we passed the signed coeff to FixRealKCM it should work like this
				if(coeffsign[i] == 1)
					vhdl << " - (" ;
				else
					vhdl << " + (" ;
#else
				vhdl << " + (" ;
#endif
				if(size>pSize) 
					vhdl << "("<< size-1 << " downto " << pSize<< " => "<< join("P",i) << of(pSize-1) << ")" << " & " ;
				vhdl << join("P", i) << ");" << endl;
			}
						
			// Rounding costs one more adder to add the half-ulp round bit. 
			// This could be avoided by pushing this bit in one of the KCM tables
			syncCycleFromSignal(join("S", n));
			manageCriticalPath(target->adderDelay(wO+1));
			
			vhdl << tab << declare("R_int", wO+1) << " <= " <<  join("S", n) << range(size-1, size-wO-1) << " + (" << zg(wO) << " & \'1\');" << endl;
			vhdl << tab << "R <= " <<  "R_int" << range(wO, 1) << ";" << endl;
		}
#endif


	};

	








	void FixSOPC::emulate(TestCase * tc) {
		// Not completely safe: we compute everything on veryLargePrec, and hope that rounding this result is equivalent to rounding the exact result
		mpfr_t t, s, rd, ru;
		mpfr_init2 (t, veryLargePrec);
		mpfr_init2 (s, veryLargePrec);
		mpfr_set_d(s, 0.0, GMP_RNDN); // initialize s to 0

		for (int i=0; i< n; i++)	{
			mpfr_t x;
			mpz_class sx = tc->getInputValue(join("X", i)); 		// get the input bit vector as an integer
			sx = bitVectorToSigned(sx, 1+msbIn[i]-lsbIn[i]); 						// convert it to a signed mpz_class
			mpfr_init2 (x, 1+msbIn[i]-lsbIn[i]);
			mpfr_set_z (x, sx.get_mpz_t(), GMP_RNDD); 				// convert this integer to an MPFR; this rounding is exact
			mpfr_mul_2si (x, x, lsbIn[i], GMP_RNDD); 						// multiply this integer by 2^-lsb to obtain a fixed-point value; this rounding is again exact

			mpfr_mul(t, x, mpcoeff[i], GMP_RNDN); 					// Here rounding possible, but precision used is ridiculously high so it won't matter
			mpfr_add(s, s, t, GMP_RNDN); 							// same comment as above
			mpfr_clears (x, NULL);
		}

		// now we should have in s the (very accurate) sum
		// round it up and down

		// make s an integer -- no rounding here
		mpfr_mul_2si (s, s, -lsbOut, GMP_RNDN);

		mpfr_init2 (rd, 1+msbOut-lsbOut);
		mpfr_init2 (ru, 1+msbOut-lsbOut);		

		mpz_class rdz, ruz;

		mpfr_get_z (rdz.get_mpz_t(), s, GMP_RNDD); 					// there can be a real rounding here
		rdz=signedToBitVector(rdz, 1+msbOut-lsbOut);
		tc->addExpectedOutput ("R", rdz);

		mpfr_get_z (ruz.get_mpz_t(), s, GMP_RNDU); 					// there can be a real rounding here	
		ruz=signedToBitVector(ruz, 1+msbOut-lsbOut);
		tc->addExpectedOutput ("R", ruz);
		
		mpfr_clears (t, s, rd, ru, NULL);
	}






	// please fill me with regression tests or corner case tests
	void FixSOPC::buildStandardTestCases(TestCaseList * tcl) {
		TestCase *tc;

#if 0
		// first few cases to check emulate()
		// All zeroes
		tc = new TestCase(this);
		for(int i=0; i<n; i++)
			tc->addInput(join("X",i), mpz_class(0) );
		emulate(tc);
		tcl->add(tc);

		// All ones (0.11111)
		tc = new TestCase(this);
		for(int i=0; i<n; i++)
			tc->addInput(join("X",i), (mpz_class(1)<<p)-1 );
		emulate(tc);
		tcl->add(tc);

		// n cases with one 0.5 and all the other 0s
		for(int i=0; i<n; i++){
			tc = new TestCase(this);
			for(int j=0; j<n; j++){
				if(i==j)
					tc->addInput(join("X",j), (mpz_class(1)<<(p-1)) );
				else
					tc->addInput(join("X",j), mpz_class(0) );					
			}
			emulate(tc);
			tcl->add(tc);
		}
#endif
	}

}
