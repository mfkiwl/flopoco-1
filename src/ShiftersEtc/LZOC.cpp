/*
  A leading zero/one counter for FloPoCo

  Author : Florent de Dinechin, Bogdan Pasca

   This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

*/

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "LZOC.hpp"

using namespace std;


namespace flopoco{

	LZOC::LZOC(Target* target, int wIn) :
		Operator(target), wIn_(wIn) {
		ostringstream currLevel, currDigit, nextLevel;

		srcFileName = "LZOC";
		setCopyrightString("Florent de Dinechin, Bogdan Pasca (2007)");

		ostringstream name; 
		name <<"LZOC_"<<wIn;
		setNameWithFreqAndUID(name.str());

		// -------- Parameter set up -----------------
		wOut_ = intlog2(wIn);
		p2wOut_ = 1<<wOut_; // no need for GMP here


		addInput ("I", wIn_);
		addInput ("OZB");
		addOutput("O", wOut_);

		vhdl << tab << declare("sozb") <<" <= OZB;" << endl;
		currLevel << "level"<<wOut_;
		ostringstream padStr;
		if (wIn_==intpow2(wOut_)) padStr<<"";
		else padStr << "& ("<<intpow2(wOut_)-wIn_-1 << " downto 0 => not(sozb))";

		vhdl << tab << declare(currLevel.str(),intpow2(wOut_)) << "<= I" << padStr.str() <<";"<<endl; //zero padding if necessary
		//each operation is formed of a comparisson folloewd by a multiplexing

		for (int i=wOut_;i>=1;i--){
			currDigit.str(""); currDigit << "digit" << i ;
			currLevel.str(""); currLevel << "level" << i;
			nextLevel.str(""); nextLevel << "level" << i-1;
			manageCriticalPath( intlog(mpz_class(getTarget()->lutInputs()), intpow2(i-1)) * getTarget()->lutDelay() + intlog(mpz_class(getTarget()->lutInputs()), intpow2(i-1))* getTarget()->localWireDelay() );

			vhdl << tab <<declare(currDigit.str()) << "<= '1' when " << currLevel.str() << "("<<intpow2(i)-1<<" downto "<<intpow2(i-1)<<") = "
				  <<"("<<intpow2(i)-1<<" downto "<<intpow2(i-1)<<" => sozb)"
				  << " else '0';"<<endl;

			if (i>1){
				manageCriticalPath( getTarget()->lutDelay() + (1 + (double(intpow2(i-1))/double(100)) )*getTarget()->localWireDelay());

				vhdl << tab << declare(nextLevel.str(),intpow2(i-1)) << "<= "<<currLevel.str() << "("<<intpow2(i-1)-1<<" downto 0) when " << currDigit.str()<<"='1' "
					  <<"else "<<currLevel.str()<<"("<<intpow2(i)-1<<" downto "<<intpow2(i-1)<<");"<<endl;
			}
		}
		//update output slack
		getOutDelayMap()["0"] = getCriticalPath();

		vhdl << tab << "O <= ";
		for (int i=wOut_;i>=1;i--){
			currDigit.str(""); currDigit << "digit" << i ;
			vhdl << currDigit.str();
			if (i==1)
				vhdl << ";"<<endl;
			else
				vhdl << " & ";
		}

	}

	LZOC::~LZOC() {}



	void LZOC::emulate(TestCase* tc)
	{
		mpz_class si   = tc->getInputValue("I");
		mpz_class sozb = tc->getInputValue("OZB");
		mpz_class so;

		int j;
		int bit = (sozb == 0) ? 0 : 1;
		for (j = 0; j < wIn_; j++)
			{
				if (mpz_tstbit(si.get_mpz_t(), wIn_ - j - 1) != bit)
					break;
			}

		so = j;
		tc->addExpectedOutput("O", so);
	}


	OperatorPtr LZOC::parseArguments(Target *target, std::vector<std::string> &args) {
		int wIn;
		UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
		return new LZOC(target, wIn);
	}


	
	void LZOC::registerFactory(){
		UserInterface::add("LZOC", // name
											 "A classical barrel shifter. The output size is computed.",
											 "ShiftersLZOCs", // category
											 "",
											 "wIn(int): input size in bits", // This string will be parsed
											 "", // no particular extra doc needed
											 LZOC::parseArguments
											 ) ;
		
	}
}
