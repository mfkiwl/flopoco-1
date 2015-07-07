/*
  Floating Point Divider for FloPoCo


  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Authors: Jeremie Detrey, Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

 */


// TODO Test even and odd significands

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"

#include "FPDiv.hpp"
#include "SelFunctionTable.hpp"

using namespace std;

namespace flopoco{

#define DEBUGVHDL 0


	FPDiv::FPDiv(Target* target, int wE, int wF, bool newVersion) :
		Operator(target), wE(wE), wF(wF) {

		int i;
		ostringstream name;

		srcFileName="FPDiv";
		name<<"FPDiv_"<<wE<<"_"<<wF;
		uniqueName_ = name.str();


		if(newVersion)
		{
			int extraBit = 0;
			extraBit+=2; //Here we'll prescale by 5/4 => 2 right extra bits
			extraBit+=1; //The sticky bit
			extraBit+=1; //The result will be in [1/2, 2[ => 1 more bit (2^0)
			extraBit+=2; //To round correctly the result
			extraBit+=3; //floor() and the bits cut to get a result depending on wF instead of nDigit (cf. the last step before normalization)

			nDigit = floor(((double)(wF + extraBit))/3);

			addFPInput ("X", wE, wF);
			addFPInput ("Y", wE, wF);
			addFPOutput("R", wE, wF);

			vhdl << tab << declare("partialFX",wF+1) << " <= \"1\" & X(" << wF-1 << " downto 0);" << endl; //IEEE norm, the first 1 is implicit
			vhdl << tab << declare("partialFY",wF+1) << " <= \"1\" & Y(" << wF-1 << " downto 0);" << endl;

			vhdl << tab << "-- exponent difference, sign and exception combination computed early, to have less bits to pipeline" << endl;

			vhdl << tab << declare("expR0", wE+2) << " <= (\"00\" & X(" << wE+wF-1 << " downto " << wF << ")) - (\"00\" & Y(" << wE+wF-1 << " downto " << wF<< "));" << endl;
			vhdl << tab << declare("sR") << " <= X(" << wE+wF << ") xor Y(" << wE+wF<< ");" << endl;
			vhdl << tab << "-- early exception handling " <<endl;
			vhdl << tab << declare("exnXY",4) << " <= X(" << wE+wF+2 << " downto " << wE+wF+1  << ") & Y(" << wE+wF+2 << " downto " << wE+wF+1 << ");" <<endl;
			vhdl << tab << "with exnXY select" <<endl;
			vhdl << tab << tab << declare("exnR0", 2) << " <= " << endl;
			vhdl << tab << tab << tab << "\"01\"  when \"0101\",                   -- normal" <<endl;
			vhdl << tab << tab << tab << "\"00\"  when \"0001\" | \"0010\" | \"0110\", -- zero" <<endl;
			vhdl << tab << tab << tab << "\"10\"  when \"0100\" | \"1000\" | \"1001\", -- overflow" <<endl;
			vhdl << tab << tab << tab << "\"11\"  when others;                   -- NaN" <<endl;



			/////////////////////////////////////////////////////////////////////////Prescaling
			vhdl << tab << " -- Prescaling" << endl;
			vhdl << tab << "with partialFY " << range(wF-1, wF-2) << " select" << endl;
			vhdl << tab << tab << declare("fY", wF+3) << " <= " << endl; //potentially *5/4 => we need 2 more bits
			vhdl << tab << tab << tab << "(\"0\" & partialFY & \"0\") + (partialFY & \"00\") when \"00\","<<endl; /////////[1/2, 5/8[*3/2 => [3/4, 15/16[
			vhdl << tab << tab << tab << "(\"00\" & partialFY) + (partialFY & \"00\") when \"01\","<<endl; /////////[5/8, 3/4[*5/4 => [25/32, 15/16[
			vhdl << tab << tab << tab << "partialFY &\"00\" when others;"<<endl; /////////no prescaling

			vhdl << tab << "with partialFY " << range(wF-1, wF-2) << " select" << endl;
			vhdl << tab << tab << declare("fX", wF+4) << " <= " << endl;
			vhdl << tab << tab << tab << "(\"00\" & partialFX & \"0\") + (\"0\" & partialFX & \"00\") when \"00\","<<endl; /////////[1/2, 5/[*3/2 => [3/4, 15/16[
			vhdl << tab << tab << tab << "(\"000\" & partialFX) + (\"0\" & partialFX & \"00\") when \"01\","<<endl; /////////[5/8, 3/4[*5/4 => [25/32, 15/16[
			vhdl << tab << tab << tab << "\"0\" & partialFX &\"00\" when others;"<<endl; /////////no prescaling

			ostringstream wInit;
			wInit << "w" << nDigit-1;
			vhdl << tab << declare(wInit.str(), wF+6) << " <=  \"00\" & fX;" << endl; //TODO : review that

			nextCycle();/////////////////////////////////////////////////////////////
			setCriticalPath(0);

			double srt4stepdelay =  2*target->lutDelay() + 2*target->localWireDelay() + target->adderDelay(wF+4);

			SelFunctionTable* table;
			table = new SelFunctionTable(target, 0.75, 1.0, 2, 5, 7, 8, 7, 5);
			addSubComponent(table);

			for(i=nDigit-1; i>=1; i--) {
				manageCriticalPath(srt4stepdelay);

				ostringstream wi, qi, qa, qb, wim1, seli, qiaTimesD, qibTimesD, wipad, wim1full, wim1fulla, tInstance;
				wi << "w" << i;						//actual partial remainder
				qi << "q" << i;						//actual quotient digit, LUT's output
				wim1 << "w" << i-1;					//partial remainder for the next iteration, = left shifted wim1full
				seli << "sel" << i;					//constructed as the wi's first 4 digits and D's first, LUT's input
				qiaTimesD << "q" << i << "aD";		//qia*D
				qibTimesD << "q" << i << "bD";		//qib*D
				wipad << "w" << i << "pad";			//1-left-shifted wi
				wim1full << "w" << i-1 << "full";	//partial remainder after this iteration, = wi+qi*D
				wim1fulla << "w" << i-1 << "fulla";	//partial remainder after this iteration, = wi+qi*D
				tInstance << "SelFunctionTable" << i;

				vhdl << tab << declare(seli.str(),7) << " <= " << wi.str() << range( wF+5, wF+1)<<" & fY"<< range(wF, wF-1) <<";" << endl;
				inPortMap (table , "X", seli.str());
				outPortMap(table , "Y", qi.str());
				vhdl << instance(table , tInstance.str());


				vhdl << tab << "with " << qi.str() << range(3, 0) << " select" << endl;
				vhdl << tab << tab << declare(qiaTimesD.str(), wF+7) << " <= "<< endl ;
				vhdl << tab << tab << tab << "\"0000\" & fY            when \"0001\" | \"0011\" | \"0101\" | \"0111\" | \"1111\" | \"1101\" | \"1011\" | \"1001\"," << endl;
				vhdl << tab << tab << tab << "\"000\" & fY & \"0\"           when \"0110\" | \"1010\"," << endl;
				vhdl << tab << tab << tab << "(" << wF+6 << " downto 0 => '0')          when others;" << endl;

				vhdl << tab << "with " << qi.str() << range(3, 0) << " select" << endl;
				vhdl << tab << tab << declare(qibTimesD.str(), wF+7) << " <= "<< endl ;
				vhdl << tab << tab << tab << "\"000\" & fY & \"0\"           when \"0010\" | \"0011\" | \"1110\" | \"1101\"," << endl;
				vhdl << tab << tab << tab << "\"00\" & fY & \"00\"           when \"0100\" | \"0101\" | \"0110\" | \"1100\" | \"1011\" | \"1010\"," << endl;
				vhdl << tab << tab << tab << "\"0\" & fY & \"000\"           when \"0111\" | \"1001\"," << endl;
				vhdl << tab << tab << tab << "(" << wF+6 << " downto 0 => '0')          when others;" << endl;

				vhdl << tab << declare(wipad.str(), wF+7) << " <= " << wi.str() << " & \"0\";" << endl;
				vhdl << tab << "with " << qi.str() << of(4) << " select" << endl;
				vhdl << tab << declare(wim1fulla.str(), wF+7 ) << " <= " << wipad.str() << " - " << qiaTimesD.str() << " when '0'," << endl;
				vhdl << tab << "      " << wipad.str() << " + " << qiaTimesD.str() << " when others;" << endl;

				vhdl << tab << "with " << qi.str() << of(3) << " select" << endl;
				vhdl << tab << declare(wim1full.str(), wF+7) << " <= " << wim1fulla.str() << " - " << qibTimesD.str() << " when '0'," << endl;
				vhdl << tab << "      " << wim1fulla.str() << " + " << qibTimesD.str() << " when others;" << endl;
				vhdl << endl;
				vhdl << tab << declare(wim1.str(),wF+6) << " <= " << wim1full.str()<<range(wF+3,0)<<" & \"00\";" << endl;
			}

			manageCriticalPath(srt4stepdelay);

			vhdl << tab << declare("q0",4) << "(3 downto 0) <= \"0000\" when  w0 = (" << wF+5 << " downto 0 => '0')" << endl;
			vhdl << tab << "             else w0(" << wF+5 << ") & \"010\";" << endl;

			for(i=nDigit-1; i>=1; i--) {
				ostringstream qi, qPi, qMi;
				qi << "q" << i;
				qPi << "qP" << i;
				qMi << "qM" << i;
				vhdl << tab << declare(qPi.str(), 3) <<" <=      " << qi.str() << "(2 downto 0);" << endl;
				vhdl << tab << declare(qMi.str(), 3)<<" <=      " << qi.str() << "(3) & \"00\";" << endl;
			}

			vhdl << tab << declare("qP0", 3) << " <= q0(2 downto 0);" << endl;
			vhdl << tab << declare("qM0", 3) << " <= q0(3)  & \"00\";" << endl;

			vhdl << tab << declare("qP", 3*nDigit) << " <= qP" << nDigit-1;
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qP" << i;
			vhdl << ";" << endl;

			vhdl << tab << declare("qM", 3*nDigit) << " <= qM" << nDigit-1 << range(1,0);
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qM" << i;
			vhdl << " & \"0\";" << endl;

			vhdl << tab << declare("fR0", 3*nDigit) << " <= qP - qM;" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////

			//The last +3 in computing nDigit is for this part
			vhdl << tab << declare("fR", wF+6) << " <= ";
			if (wF % 3 == 1) //2 extra bit
				vhdl << "fR0(" << 3*nDigit-1 << " downto 3) & (fR0(0) or fR0(1) or fR0(2)); " << endl;

			else if (wF % 3 == 2)// 1 extra bits
				vhdl << "fR0(" << 3*nDigit-1 << " downto 2) & (fR0(0) or fR0(1)); " << endl;

			else // 3 extra bit
				vhdl << "fR0(" << 3*nDigit-1 << " downto 4) & (fR0(0) or fR0(1) or fR0(2) or fR0(3)); " << endl;


			vhdl << tab << "-- normalisation" << endl;
			vhdl << tab << "with fR(" << wF+4 << ") select" << endl;

			vhdl << tab << tab << declare("fRn1", wF+4) << " <= fR(" << wF+4 << " downto 2) & (fR(0) or fR(1)) when '1'," << endl;
			vhdl << tab << tab << "        fR(" << wF+3 << " downto 0)          when others;" << endl;

			vhdl << tab << declare("expR1", wE+2) << " <= expR0"
				 << " + (\"000\" & (" << wE-2 << " downto 1 => '1') & fR(" << wF+4 << ")); -- add back bias" << endl;


			vhdl << tab << declare("round") << " <= fRn1(2) and (fRn1(0) or fRn1(1) or fRn1(3)); -- fRn1(0) is the sticky bit" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////
			vhdl << tab << "-- final rounding" <<endl;
			vhdl << tab <<  declare("expfrac", wE+wF+2) << " <= "
				 << "expR1 & fRn1(" << wF+2 << " downto 3) ;" << endl;
			vhdl << tab << declare("expfracR", wE+wF+2) << " <= "
				 << "expfrac + ((" << wE+wF+1 << " downto 1 => '0') & round);" << endl;
			vhdl << tab <<  declare("exnR", 2) << " <=      \"00\"  when expfracR(" << wE+wF+1 << ") = '1'   -- underflow" <<endl;
			vhdl << tab << "        else \"10\"  when  expfracR(" << wE+wF+1 << " downto " << wE+wF << ") =  \"01\" -- overflow" <<endl;
			vhdl << tab << "        else \"01\";      -- 00, normal case" <<endl;


			vhdl << tab << "with exnR0 select" <<endl;
			vhdl << tab << tab << declare("exnRfinal", 2) << " <= " <<endl;
			vhdl << tab << tab << tab << "exnR   when \"01\", -- normal" <<endl;
			vhdl << tab << tab << tab << "exnR0  when others;" <<endl;
			vhdl << tab << "R <= exnRfinal & sR & "
				 << "expfracR(" << wE+wF-1 << " downto 0);" <<endl;
		}


		else //TODO : the old version is using 5-input's LUTs, try to fit in 4-input's LUTs (same as above : select qA and qB and make a 2-levels addition)
		{
			// -------- Parameter set up -----------------
			nDigit = (wF+6) >> 1;

			addFPInput ("X", wE, wF);
			addFPInput ("Y", wE, wF);
			addFPOutput("R", wE, wF);

			vhdl << tab << declare("fX",wF+1) << " <= \"1\" & X(" << wF-1 << " downto 0);" << endl;
			vhdl << tab << declare("fY",wF+1) << " <= \"1\" & Y(" << wF-1 << " downto 0);" << endl;

			vhdl << tab << "-- exponent difference, sign and exception combination computed early, to have less bits to pipeline" << endl;

			vhdl << tab << declare("expR0", wE+2) << " <= (\"00\" & X(" << wE+wF-1 << " downto " << wF << ")) - (\"00\" & Y(" << wE+wF-1 << " downto " << wF<< "));" << endl;
			vhdl << tab << declare("sR") << " <= X(" << wE+wF << ") xor Y(" << wE+wF<< ");" << endl;
			vhdl << tab << "-- early exception handling " <<endl;
			vhdl << tab << declare("exnXY",4) << " <= X(" << wE+wF+2 << " downto " << wE+wF+1  << ") & Y(" << wE+wF+2 << " downto " << wE+wF+1 << ");" <<endl;
			vhdl << tab << "with exnXY select" <<endl;
			vhdl << tab << tab << declare("exnR0", 2) << " <= " << endl;
			vhdl << tab << tab << tab << "\"01\"  when \"0101\",                   -- normal" <<endl;
			vhdl << tab << tab << tab << "\"00\"  when \"0001\" | \"0010\" | \"0110\", -- zero" <<endl;
			vhdl << tab << tab << tab << "\"10\"  when \"0100\" | \"1000\" | \"1001\", -- overflow" <<endl;
			vhdl << tab << tab << tab << "\"11\"  when others;                   -- NaN" <<endl;
			vhdl << tab << " -- compute 3Y" << endl;
			vhdl << tab << declare("fYTimes3",wF+3) << " <= (\"00\" & fY) + (\"0\" & fY & \"0\");" << endl; // TODO an IntAdder here

			ostringstream wInit;
			wInit << "w"<<nDigit-1;
			vhdl << tab << declare(wInit.str(), wF+3) <<" <=  \"00\" & fX;" << endl;

			nextCycle();/////////////////////////////////////////////////////////////
			setCriticalPath(0);

			double srt4stepdelay =  2*target->lutDelay() + 2*target->localWireDelay() + target->adderDelay(wF+4);

			SelFunctionTable* table;
			table = new SelFunctionTable(target, 0.5, 1.0, 1, 4, 3, 4, 5, 3);
			addSubComponent(table);

			for(i=nDigit-1; i>=1; i--) {
				manageCriticalPath(srt4stepdelay);

				ostringstream wi, qi, wim1, seli, qiTimesD, wipad, wim1full, tInstance;
				wi << "w" << i;						//actual partial remainder
				qi << "q" << i;						//actual quotient digit, LUT's output
				wim1 << "w" << i-1;					//partial remainder for the next iteration, = left shifted wim1full
				seli << "sel" << i;					//constructed as the wi's first 4 digits and D's first, LUT's input
				qiTimesD << "q" << i << "D";		//qi*D
				wipad << "w" << i << "pad";			//1-left-shifted wi
				wim1full << "w" << i-1 << "full";	//partial remainder after this iteration, = wi+qi*D
				tInstance << "SelFunctionTable" << i;

				/*
					Detailed algorithm :
					*	building seli for the selection function
						seli = wi (25 downto 22) & fY(22), i.e. the first 4 digits of the remainder and the first useful digit of the divisor
					*	deducing the value of qi out of seli
					*	deducing the value of qi*D out of qi
						qi is bounded to [-3,3], so you can compute qi*D in 1 addition
					*	D : 24 bits, qi : 3 bits => qi*D : potentially 27 bits
						or wi is 26 bits long
						=>computing wipad = wi left shifted (27 bits)
					*	computing the remainder of this step
						wi-1full = wi-qi*D
					*	left shifting wi-1full to obtain wi-1, next partial remainder to work on
				*/

				vhdl << tab << declare(seli.str(),5) << " <= " << wi.str() << range( wF+2, wF-1)<<" & fY"<<of(wF-1)<<";" << endl;
				inPortMap (table , "X", seli.str());
				outPortMap(table , "Y", qi.str());
				vhdl << instance(table , tInstance.str());

				vhdl << endl;
				vhdl << tab << "with " << qi.str() << " select" << endl;
				vhdl << tab << tab << declare(qiTimesD.str(),wF+4) << " <= "<< endl ;
				vhdl << tab << tab << tab << "\"000\" & fY            when \"001\" | \"111\"," << endl;
				vhdl << tab << tab << tab << "\"00\" & fY & \"0\"       when \"010\" | \"110\"," << endl;
				vhdl << tab << tab << tab << "\"0\" & fYTimes3        when \"011\" | \"101\"," << endl;
				vhdl << tab << tab << tab << "(" << wF+3 << " downto 0 => '0')  when others;" << endl;
				vhdl << endl;
				vhdl << tab << declare(wipad.str(), wF+4) << " <= " << wi.str() << " & \"0\";" << endl;
				vhdl << tab << "with " << qi.str() << "(2) select" << endl;
				vhdl << tab << declare(wim1full.str(), wF+4) << "<= " << wipad.str() << " - " << qiTimesD.str() << " when '0'," << endl;
				vhdl << tab << "      " << wipad.str() << " + " << qiTimesD.str() << " when others;" << endl;
				vhdl << endl;
				vhdl << tab << declare(wim1.str(),wF+3) << " <= " << wim1full.str()<<range(wF+1,0)<<" & \"0\";" << endl;
			}


			manageCriticalPath(srt4stepdelay);

			vhdl << tab << declare("q0",3) << "(2 downto 0) <= \"000\" when  w0 = (" << wF+2 << " downto 0 => '0')" << endl;
			vhdl << tab << "             else w0(" << wF+2 << ") & \"10\";" << endl;

			for(i=nDigit-1; i>=1; i--) {
				ostringstream qi, qPi, qMi;
				qi << "q" << i;
				qPi << "qP" << i;
				qMi << "qM" << i;
				vhdl << tab << declare(qPi.str(), 2) <<" <=      " << qi.str() << "(1 downto 0);" << endl;
				vhdl << tab << declare(qMi.str(), 2)<<" <=      " << qi.str() << "(2) & \"0\";" << endl;
			}

			vhdl << tab << declare("qP0", 2) << " <= q0(1 downto 0);" << endl;
			vhdl << tab << declare("qM0", 2) << " <= q0(2)  & \"0\";" << endl;

			vhdl << tab << declare("qP", 2*nDigit) << " <= qP" << nDigit-1;
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qP" << i;
			vhdl << ";" << endl;

			vhdl << tab << declare("qM", 2*nDigit) << " <= qM" << nDigit-1 << "(0)";
			for (i=nDigit-2; i>=0; i--)
				vhdl << " & qM" << i;
			vhdl << " & \"0\";" << endl;


			// TODO an IntAdder here
			vhdl << tab << declare("fR0", 2*nDigit) << " <= qP - qM;" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////


			vhdl << tab << declare("fR", wF+4) << " <= ";
			if (1 == (wF & 1) ) // odd wF
				vhdl << "fR0(" << 2*nDigit-1 << " downto 1);  -- odd wF" << endl;
			else
				vhdl << "fR0(" << 2*nDigit-1 << " downto 3)  & (fR0(2) or fR0(1));  -- even wF, fixing the round bit" << endl;


			vhdl << tab << "-- normalisation" << endl;
			vhdl << tab << "with fR(" << wF+3 << ") select" << endl;

			vhdl << tab << tab << declare("fRn1", wF+2) << " <= fR(" << wF+2 << " downto 2) & (fR(1) or fR(0)) when '1'," << endl;
			vhdl << tab << tab << "        fR(" << wF+1 << " downto 0)                    when others;" << endl;

			vhdl << tab << declare("expR1", wE+2) << " <= expR0"
				  << " + (\"000\" & (" << wE-2 << " downto 1 => '1') & fR(" << wF+3 << ")); -- add back bias" << endl;



			vhdl << tab << declare("round") << " <= fRn1(1) and (fRn1(2) or fRn1(0)); -- fRn1(0) is the sticky bit" << endl;

			nextCycle();///////////////////////////////////////////////////////////////////////
			vhdl << tab << "-- final rounding" <<endl;
			vhdl << tab <<  declare("expfrac", wE+wF+2) << " <= "
				 << "expR1 & fRn1(" << wF+1 << " downto 2) ;" << endl;
			vhdl << tab << declare("expfracR", wE+wF+2) << " <= "
				 << "expfrac + ((" << wE+wF+1 << " downto 1 => '0') & round);" << endl;
			vhdl << tab <<  declare("exnR", 2) << " <=      \"00\"  when expfracR(" << wE+wF+1 << ") = '1'   -- underflow" <<endl;
			vhdl << tab << "        else \"10\"  when  expfracR(" << wE+wF+1 << " downto " << wE+wF << ") =  \"01\" -- overflow" <<endl;
			vhdl << tab << "        else \"01\";      -- 00, normal case" <<endl;


			vhdl << tab << "with exnR0 select" <<endl;
			vhdl << tab << tab << declare("exnRfinal", 2) << " <= " <<endl;
			vhdl << tab << tab << tab << "exnR   when \"01\", -- normal" <<endl;
			vhdl << tab << tab << tab << "exnR0  when others;" <<endl;
			vhdl << tab << "R <= exnRfinal & sR & "
				 << "expfracR(" << wE+wF-1 << " downto 0);" <<endl;
		}
	}

	FPDiv::~FPDiv() {
	}



	void FPDiv::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");

		/* Compute correct value */
		FPNumber fpx(wE, wF), fpy(wE, wF);
		fpx = svX;
		fpy = svY;
		mpfr_t x, y, r;
		mpfr_init2(x, 1+wF);
		mpfr_init2(y, 1+wF);
		mpfr_init2(r, 1+wF);
		fpx.getMPFR(x); // fake 0
		fpy.getMPFR(y);
		mpfr_div(r, x, y, GMP_RNDN);
		FPNumber  fpr(wE, wF, r);

		/* Set outputs */
		mpz_class svR = fpr.getSignalValue();
		tc->addExpectedOutput("R", svR);
		mpfr_clears(x, y, r, NULL);
	}



	void FPDiv::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		// Regression tests
		tc = new TestCase(this);
		tc->addFPInput("X", 1.0);
		tc->addFPInput("Y", FPNumber::plusDirtyZero);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", FPNumber::minusDirtyZero);
		tc->addFPInput("Y", FPNumber::plusInfty);
		emulate(tc);
		tcl->add(tc);


	}

	OperatorPtr FPDiv::parseArguments(Target *target, const vector<string> &args) {
		int wE = UserInterface::checkStrictlyPositiveInt(args, "wE");
		int wF = UserInterface::checkStrictlyPositiveInt(args, "wF");
		bool radix8 = UserInterface::checkBoolean(args, "radix8");
		return new FPDiv(target, wE, wF, radix8);
	}

	void FPDiv::registerFactory(){
		UserInterface::add("FPDiv", // name
											 "A correctly rounded floating-point division.",
											 "operator; floating point; dividers and square roots", // categories
											 "wE(int): exponent size in bits; \
wF(int): mantissa size in bits; \
radix8(bool)=true: if true, the division will be implemented in radix 8, if false in radix 4;",
											  "The algorithm used here is the division by digit recurrence. For more details, check <a href=\"http://www.cs.ucla.edu/digital_arithmetic/files/ch5.pdf\">this pdf.</a>",
                        					 FPDiv::parseArguments
											 ) ;

	}

}
