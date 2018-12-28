#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "FixHalfSine.hpp"

using namespace std;

namespace flopoco{


	FixHalfSine::FixHalfSine(OperatorPtr parentOp_, Target* target_, int lsbIn_, int lsbOut_, int N_) :
		FixFIR(parentOp_, target_, lsbIn_, lsbOut_), N(N_)
	{
		srcFileName="FixHalfSine";
		
		ostringstream name;
		name << "FixHalfSine_" << -lsbIn  << "_" << -lsbOut  << "_" << N;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Louis Besème, Florent de Dinechin, Matei Istoan (2014-2018)");

		symmetry = 1; // this is a FIR attribute, Half-sine is a symmetric filter
		rescale=true; // also a FIR attribute
		// define the coefficients
		for (int i=1; i<2*N; i++) {
			ostringstream c;
			c <<  "sin(pi*" << i << "/" << 2*N << ")";
			coeff.push_back(c.str());
		}

		buildVHDL();
	};

	FixHalfSine::~FixHalfSine(){}

	OperatorPtr FixHalfSine::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int lsbIn;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		int n;
		UserInterface::parseStrictlyPositiveInt(args, "n", &n);
		OperatorPtr tmpOp = new FixHalfSine(parentOp, target, lsbIn, lsbOut, n);
		return tmpOp;
	}

	void FixHalfSine::registerFactory(){
		UserInterface::add("FixHalfSine", // name
											 "A generator of fixed-point Half-Sine filters",
											 "FiltersEtc", // categories
											 "",
											 "lsbIn(int): weight of the integer size in bits;\
											  lsbOut(int): integer size in bits;\
                        n(int): filter order (number of taps will be 2n)",
											 "For more details, see <a href=\"bib/flopoco.html#DinIstoMas2014-SOPCJR\">this article</a>.",
											 FixHalfSine::parseArguments
											 ) ;
	}

}


