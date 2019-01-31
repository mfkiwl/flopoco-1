// TODO: repair FixFIR, FixIIR, FixComplexKCM
/*
 * A faithful multiplier by a real constant, using a variation of the KCM method
 *
 * This file is part of the FloPoCo project developed by the Arenaire
 * team at Ecole Normale Superieure de Lyon
 * 
 * Authors : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr
 * 			 3IF-Dev-Team-2015
 *
 * Initial software.
 * Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 
 * 2008-2011.
 * All rights reserved.
 */
/*

Remaining 1-ulp bug:
flopoco verbose=3 FixRealShiftAdd lsbIn=-8 msbIn=0 lsbOut=-7 constant="0.16" signedInput=true TestBench
It is the limit case of removing one table altogether because it contributes nothng.
I don't really understand

*/

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)


#include "pagsuite/types.h"

#include "../Operator.hpp"

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <sollya.h>
#include "../utils.hpp"
#include "FixRealShiftAdd.hpp"
#include "../IntAddSubCmp/IntAdder.hpp"

#include "pagsuite/log2_64.h"
#include "pagsuite/csd.h"
#include "pagsuite/rpag.h"

#include "adder_cost.hpp"
#include "error_comp_graph.hpp"
#include "WordLengthCalculator.hpp"
#include "IntConstMultShiftAddTypes.hpp"

using namespace std;


namespace flopoco{




	
	//standalone operator
	FixRealShiftAdd::FixRealShiftAdd(OperatorPtr parentOp, Target* target, bool signedInput_, int msbIn_, int lsbIn_, int lsbOut_, string constant_, double targetUlpError_):
		Operator(parentOp, target),
		thisOp(this),
		signedInput(signedInput_),
		msbIn(msbIn_),
		lsbIn(lsbIn_),
		lsbOut(lsbOut_),
		constant(constant_),
		targetUlpError(targetUlpError_)
	{
		vhdl << "-- This operator multiplies by " << constant << endl;

		srcFileName = "FixRealShiftAdd";
		setNameWithFreqAndUID("FixRealShiftAdd");

		useNumericStd();

		// Convert the input string into a sollya evaluation tree
		sollya_obj_t node;
		node = sollya_lib_parse_string(constant.c_str());
		/* If  parse error throw an exception */
		if (sollya_lib_obj_is_error(node))
		{
			ostringstream error;
			error << srcFileName << ": Unable to parse string " <<
				  constant << " as a numeric constant" << endl;
			throw error.str();
		}

		mpfr_init2(mpC, 10000);
		mpfr_init2(absC, 10000);

		sollya_lib_get_constant(mpC, node);

		//if negative constant, then set negativeConstant
		negativeConstant = (mpfr_cmp_si(mpC, 0) < 0 ? true : false);

		signedOutput = negativeConstant || signedInput;
		mpfr_abs(absC, mpC, GMP_RNDN);

		REPORT(DEBUG, "Constant evaluates to " << mpfr_get_d(mpC, GMP_RNDN));


		//compute the logarithm only of the constants
		mpfr_t log2C;
		mpfr_init2(log2C, 100); // should be enough for anybody
		mpfr_log2(log2C, absC, GMP_RNDN);
		msbC = mpfr_get_si(log2C, GMP_RNDU);
		mpfr_clears(log2C, NULL);

		// Now we can check when this is a multiplier by 0: either because the it is zero, or because it is close enough
		if (mpfr_zero_p(mpC) != 0)
		{
			msbOut = lsbOut; // let us return a result on one bit, why not.
			REPORT(INFO, "It seems somebody asked for a multiplication by 0. We can do that.");
			return;
		}

		msbOut = msbIn + msbC;
		int wIn = msbIn - lsbIn + 1;

		REPORT(INFO, "Output precisions: msbOut=" << msbOut << ", lsbOut=" << lsbOut);

		mpfr_t mpOp1, mpOp2; //temporary variables for operands
		mpfr_init2(mpOp1, 100);
		mpfr_init2(mpOp2, 100);

		//compute error bounds of the result (epsilon_max)
		mpfr_t mpEpsilonMax;
		mpfr_init2(mpEpsilonMax, 100);
		mpfr_set_si(mpOp1, 2, GMP_RNDN);
//		mpfr_set_si(mpOp2,lsbOut,GMP_RNDN);
//		mpfr_set_si(mpOp2,lsbOut-1,GMP_RNDN); //as we don't round the result, take one bit more here!!
		mpfr_set_si(mpOp2, lsbOut - 2, GMP_RNDN); //one additional guard bit required??

		mpfr_pow(mpEpsilonMax, mpOp1, mpOp2, GMP_RNDN); //2^(lsbOut-1)
		ios::fmtflags old_settings = cout.flags();
		REPORT(INFO, "Epsilon max=" << std::scientific << mpfr_get_d(mpEpsilonMax, GMP_RNDN));
		cout.flags(old_settings);

		//compute the different integer representations of the constant
		int q = 3;
		//mx = msbIn
		//lR = lsbOut
		mpfr_t s;
		mpfr_init2(s, 64);
		mpfr_set_si(s, msbIn - lsbOut + q + 1, GMP_RNDN);

		cout << "coefficient word size = " << msbC + msbIn - lsbOut + q + 1 << endl;
		mpfr_t mpCInt;
		mpfr_init2(mpCInt, msbC + msbIn - lsbOut + q + 1); //msbC-lsbOut+q+1

		int noOfFullAddersBest = INT32_MAX;
		mpz_class mpzCIntBest;
		int shiftTotalBest;
		string adderGraphStrBest;
		PAGSuite::adder_graph_t adderGraphBest;
		string trunactionStrBest;
		IntConstMultShiftAdd_TYPES::TruncationRegister truncationRegBest("");

		for (int k = -(1 << q); k <= (1 << q); k++)
		{
			mpfr_rnd_t roundingDirection;
			if (k > 0)
				roundingDirection = MPFR_RNDD;
			else
				roundingDirection = MPFR_RNDU;

			mpfr_t two_pow_s;
			mpfr_init2(two_pow_s, 100); //msbIn-lsbOut+q+1
			mpfr_set_si(mpOp1, 2, GMP_RNDN);
			mpfr_pow(two_pow_s, mpOp1, s, GMP_RNDN);
			mpfr_mul(mpCInt, mpC, two_pow_s, roundingDirection);

			mpfr_t mpk;
			mpfr_init2(mpk, 64);
			mpfr_set_si(mpk, k, GMP_RNDN);
			mpfr_add(mpCInt, mpCInt, mpk, roundingDirection);

			mpz_class mpzCInt;
			mpfr_get_z(mpzCInt.get_mpz_t(), mpCInt, GMP_RNDN);

			mpz_class isEven = (mpzCInt & 0x01) != 1;
			int shift = 0;
			while (isEven.get_ui())
			{
				mpzCInt /= 2;
				mpfr_div_ui(mpCInt, mpCInt, 2, GMP_RNDN);
				shift++;
				isEven = (mpzCInt & 0x01) != 1;
			}

			//compute epsilons
			//compute mpEpsilonCoeff = C - Cint * 2^(-(wIn+q-shift));
			mpfr_t mpEpsilonCoeff;
			mpfr_init2(mpEpsilonCoeff, 100);
			mpfr_set_si(mpOp1, 2, GMP_RNDN);
			mpfr_set_si(mpOp2, -(wIn + q - shift), GMP_RNDN);
			mpfr_pow(mpOp1, mpOp1, mpOp2, GMP_RNDN);
			mpfr_mul(mpOp1, mpCInt, mpOp1, GMP_RNDN);
			mpfr_sub(mpEpsilonCoeff, mpC, mpOp1, GMP_RNDN);

			//compute mpEpsilonMult = mpEpsilonMax - abs(mpEpsilonCoeff);
			mpfr_t mpEpsilonMult;
			mpfr_init2(mpEpsilonMult, 100);
			mpfr_abs(mpOp1, mpEpsilonCoeff, GMP_RNDN);
			mpfr_sub(mpEpsilonMult, mpEpsilonMax, mpOp1, GMP_RNDN);

			//compute mpEpsilonCoeffNorm = mpEpsilonCoeff*2^(wIn+q-shift-lsbOut);
			mpfr_t mpEpsilonCoeffNorm;
			mpfr_init2(mpEpsilonCoeffNorm, 100);
			mpfr_set_si(mpOp1, 2, GMP_RNDN);
			mpfr_set_si(mpOp2, wIn + q - shift - lsbOut, GMP_RNDN);
			mpfr_pow(mpOp1, mpOp1, mpOp2, GMP_RNDN);
			mpfr_mul(mpEpsilonCoeffNorm, mpEpsilonCoeff, mpOp1, GMP_RNDN);

			//compute mpEpsilonMultNorm = mpEpsilonMult*2^(wIn+q-shift-lsbOut);
			mpfr_t mpEpsilonMultNorm;
			mpfr_init2(mpEpsilonMultNorm, 100);
			mpfr_mul(mpEpsilonMultNorm, mpEpsilonMult, mpOp1, GMP_RNDN);

			int shiftTotal = mpfr_get_si(s, GMP_RNDN) - shift;

			ios::fmtflags old_settings = cout.flags();
			REPORT(INFO, "k=" << k << ", constant = " << mpzCInt << " / 2^" << shiftTotal
							  << ", mpEpsilonCoeff=" << std::scientific << mpfr_get_d(mpEpsilonCoeff, GMP_RNDN)
							  << ", mpEpsilonMult=" << std::scientific << mpfr_get_d(mpEpsilonMult, GMP_RNDN)
							  << ", mpEpsilonCoeffNorm=" << std::fixed << mpfr_get_d(mpEpsilonCoeffNorm, GMP_RNDN)
							  << ", mpEpsilonMultNorm=" << std::fixed << mpfr_get_d(mpEpsilonMultNorm, GMP_RNDN));
			cout.flags(old_settings);

			mpfr_t mpEpsilonMultNormInt;
			mpfr_init2(mpEpsilonMultNormInt, 100);
			mpfr_floor(mpEpsilonMultNormInt, mpEpsilonMultNorm);
			long epsilonMultNormInt = mpfr_get_si(mpEpsilonMultNormInt, GMP_RNDN); //should be mpz type instead

			PAGSuite::adder_graph_t adderGraph;
			string adderGraphStr;
			computeAdderGraph(adderGraph, adderGraphStr, (PAGSuite::int_t) mpzCInt.get_si());

			int noOfFullAdders = IntConstMultShiftAdd_TYPES::getGraphAdderCost(adderGraph, wIn, false);
			REPORT(INFO, "  adder graph before truncation requires " << noOfFullAdders << " full adders");


			map<pair<mpz_class, int>, vector<int> > wordSizeMap;

			WordLengthCalculator wlc = WordLengthCalculator(adderGraph, wIn, epsilonMultNormInt);
			wordSizeMap = wlc.optimizeTruncation();
			REPORT(DEBUG, "Finished computing word sizes of truncated MCM");
			if (UserInterface::verbose >= DETAILED)
			{
				for (auto &it : wordSizeMap)
				{
					std::cout << "(" << it.first.first << ", " << it.first.second << "): ";
					for (auto &itV : it.second)
						std::cout << itV << " ";
					std::cout << std::endl;
				}
			}
			IntConstMultShiftAdd_TYPES::TruncationRegister truncationReg(wordSizeMap);

			string trunactionStr = truncationReg.convertToString();
			REPORT(INFO, "  truncation: " << trunactionStr);
			noOfFullAdders = IntConstMultShiftAdd_TYPES::getGraphAdderCost(adderGraph, wIn, false, truncationReg);
			REPORT(INFO, "  adder graph after truncation requires " << noOfFullAdders << " full adders");

			IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraph, "", wIn, cout);
			IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraph, truncationReg, wIn, cout);

			if (noOfFullAdders < noOfFullAddersBest)
			{
				noOfFullAddersBest = noOfFullAdders;
				mpzCIntBest = mpzCInt;
				shiftTotalBest = shiftTotal;
				adderGraphStrBest = adderGraphStr;
				adderGraphBest = adderGraph;
				trunactionStrBest = trunactionStr;
				truncationRegBest = truncationReg;
			}

		}
		REPORT(INFO, "best solution found for coefficient " << mpzCIntBest << " / 2^" << shiftTotalBest << " with "
															<< noOfFullAddersBest << " full adders")
		REPORT(INFO, "  adder graph " << adderGraphStrBest);
		REPORT(INFO, "  truncations:" << trunactionStrBest);

		IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraphBest, "", wIn, cout);
		IntConstMultShiftAdd_TYPES::print_aligned_word_graph(adderGraphBest, truncationRegBest, wIn, cout);

		//VHDL code generation:
		int wOut = msbOut - lsbOut + 1;

		mpfr_t tmp;
		mpfr_init2(tmp, 100);
		mpfr_set_z(tmp, mpzCIntBest.get_mpz_t(), GMP_RNDN);
		mpfr_log2(tmp, tmp, GMP_RNDN);
		mpfr_ceil(tmp, tmp);
		long wC = mpfr_get_si(tmp, GMP_RNDN);

		int wConstMultRes = wIn + wC;

		addInput("X", wIn);
		addOutput("R", wOut);

		declare("tmp", wOut + 1);
		declare("constMultRes", wConstMultRes);

		stringstream parameters;
		parameters << "wIn=" << wIn << " graph=" << adderGraphStrBest << " truncations=" << trunactionStrBest;
		string inPortMaps = "x_in0=>X";
		stringstream outPortMaps;
		outPortMaps << "x_out0_c" << mpzCIntBest << "=>constMultRes";

		cout << "outPortMaps: " << outPortMaps.str() << endl;
		newInstance("IntConstMultShiftAdd", "IntConstMultShiftAddComponent", parameters.str(), inPortMaps,
					outPortMaps.str());

		bool doProperRouding=true;
		if(doProperRouding)
		{
			stringstream one;
			for (int i = 0; i < wOut; i++)
				one << "0";
			one << "1";
			vhdl << tab << "tmp <= std_logic_vector(signed(constMultRes(" << wConstMultRes - 1 << " downto "
				 << wConstMultRes - wOut - 1 << ")) + \"" << one.str() << "\");" << endl;
			vhdl << tab << "R <= tmp(" << wOut << " downto " << 1 << ");" << endl;
		}
		else
		{
			vhdl << tab << "R <= constMultRes(" << wConstMultRes-1 << " downto " << wConstMultRes-wOut << ");" << endl;
		}
	}


	bool FixRealShiftAdd::computeAdderGraph(PAGSuite::adder_graph_t &adderGraph, string &adderGraphStr, long long int coefficient)
	{
		set<PAGSuite::int_t> target_set;

		target_set.insert(coefficient);

		int depth = PAGSuite::log2c_64(PAGSuite::nonzeros(coefficient));

//		REPORT(INFO, "depth=" << depth);

		PAGSuite::rpag *rpag = new PAGSuite::rpag(); //default is RPAG with 2 input adders

		for(PAGSuite::int_t t : target_set)
			rpag->target_set->insert(t);

		if(depth > 3)
		{
			REPORT(DEBUG, "depth is 4 or more, limit search limit to 1");
			rpag->search_limit = 1;
		}
		if(depth > 4)
		{
			REPORT(DEBUG, "depth is 5 or more, limit MSD permutation limit");
			rpag->msd_digit_permutation_limit = 1000;
		}
		PAGSuite::global_verbose = UserInterface::verbose-2; //set rpag to one less than verbose of FloPoCo

		PAGSuite::cost_model_t cost_model = PAGSuite::LL_FPGA;// with default value
		rpag->input_wordsize = msbIn-lsbIn;
		rpag->set_cost_model(cost_model);
		rpag->optimize();

		vector<set<PAGSuite::int_t>> pipeline_set = rpag->get_best_pipeline_set();

		list<PAGSuite::realization_row<PAGSuite::int_t> > rpagAdderGraph;
		PAGSuite::pipeline_set_to_adder_graph(pipeline_set, rpagAdderGraph, true, rpag->get_c_max());
		PAGSuite::append_targets_to_adder_graph(pipeline_set, rpagAdderGraph, target_set);

		adderGraphStr = PAGSuite::output_adder_graph(rpagAdderGraph,true);

		REPORT(INFO, "  adderGraphStr=" << adderGraphStr);

		if(UserInterface::verbose >= 3)
			adderGraph.quiet = false; //enable debug output
		else
			adderGraph.quiet = true; //disable debug output, except errors

		REPORT( DETAILED, "parse graph...")
		bool validParse = adderGraph.parse_to_graph(adderGraphStr);

		if(validParse)
		{

			REPORT(DETAILED, "check graph...")
			adderGraph.check_and_correct(adderGraphStr);

			if (UserInterface::verbose >= DETAILED)
				adderGraph.print_graph();
			adderGraph.drawdot("pag_input_graph.dot");

//			REPORT(INFO, "  adderGraph=" << adderGraph.get_adder_graph_as_string());

			return true;
		}
		return false;
	}


	// TODO manage correctly rounded cases, at least the powers of two
	// To have MPFR work in fix point, we perform the multiplication in very
	// large precision using RN, and the RU and RD are done only when converting
	// to an int at the end.
	void FixRealShiftAdd::emulate(TestCase* tc)
	{
		// Get I/O values
		mpz_class svX = tc->getInputValue("X");
		bool negativeInput = false;
		int wIn=msbIn-lsbIn+1;
		int wOut=msbOut-lsbOut+1;
		
		// get rid of two's complement
		if(signedInput)	{
			if ( svX > ( (mpz_class(1)<<(wIn-1))-1) )	 {
				svX -= (mpz_class(1)<<wIn);
				negativeInput = true;
			}
		}
		
		// Cast it to mpfr 
		mpfr_t mpX; 
		mpfr_init2(mpX, msbIn-lsbIn+2);	
		mpfr_set_z(mpX, svX.get_mpz_t(), GMP_RNDN); // should be exact
		
		// scale appropriately: multiply by 2^lsbIn
		mpfr_mul_2si(mpX, mpX, lsbIn, GMP_RNDN); //Exact
		
		// prepare the result
		mpfr_t mpR;
		mpfr_init2(mpR, 10*wOut);
		
		// do the multiplication
		mpfr_mul(mpR, mpX, mpC, GMP_RNDN);
		
		// scale back to an integer
		mpfr_mul_2si(mpR, mpR, -lsbOut, GMP_RNDN); //Exact
		mpz_class svRu, svRd;
		
		mpfr_get_z(svRd.get_mpz_t(), mpR, GMP_RNDD);
		mpfr_get_z(svRu.get_mpz_t(), mpR, GMP_RNDU);

		//		cout << " emulate x="<< svX <<"  before=" << svRd;
 		if(negativeInput != negativeConstant)		{
			svRd += (mpz_class(1) << wOut);
			svRu += (mpz_class(1) << wOut);
		}
		//		cout << " emulate after=" << svRd << endl;

		//Border cases
		if(svRd > (mpz_class(1) << wOut) - 1 )		{
			svRd = 0;
		}

		if(svRu > (mpz_class(1) << wOut) - 1 )		{
			svRu = 0;
		}

		tc->addExpectedOutput("R", svRd);
		tc->addExpectedOutput("R", svRu);

		// clean up
		mpfr_clears(mpX, mpR, NULL);
	}


	TestList FixRealShiftAdd::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests

			vector<string> constantList; // The list of constants we want to test
			constantList.push_back("\"0\"");
			constantList.push_back("\"0.125\"");
			constantList.push_back("\"-0.125\"");
			constantList.push_back("\"4\"");
			constantList.push_back("\"-4\"");
			constantList.push_back("\"log(2)\"");
			constantList.push_back("-\"log(2)\"");
			constantList.push_back("\"0.00001\"");
			constantList.push_back("\"-0.00001\"");
			constantList.push_back("\"0.0000001\"");
			constantList.push_back("\"-0.0000001\"");
			constantList.push_back("\"123456\"");
			constantList.push_back("\"-123456\"");

			for(int wIn=3; wIn<16; wIn+=4) { // test various input widths
				for(int lsbIn=-1; lsbIn<2; lsbIn++) { // test various lsbIns
					string lsbInStr = to_string(lsbIn);
					string msbInStr = to_string(lsbIn+wIn);
					for(int lsbOut=-1; lsbOut<2; lsbOut++) { // test various lsbIns
						string lsbOutStr = to_string(lsbOut);
						for(int signedInput=0; signedInput<2; signedInput++) {
							string signedInputStr = to_string(signedInput);
							for(auto c:constantList) { // test various constants
								paramList.push_back(make_pair("lsbIn",  lsbInStr));
								paramList.push_back(make_pair("lsbOut", lsbOutStr));
								paramList.push_back(make_pair("msbIn",  msbInStr));
								paramList.push_back(make_pair("signedInput", signedInputStr));
								paramList.push_back(make_pair("constant", c));
								testStateList.push_back(paramList);
								paramList.clear();
							}
						}
					}
				}
			}
		}
		else     
		{
				// finite number of random test computed out of index
		}	

		return testStateList;
	}

	OperatorPtr FixRealShiftAdd::parseArguments(OperatorPtr parentOp, Target* target, std::vector<std::string> &args)
	{
		int lsbIn, lsbOut, msbIn;
		bool signedInput;
		double targetUlpError;
		string constant;
		UserInterface::parseInt(args, "lsbIn", &lsbIn);
		UserInterface::parseString(args, "constant", &constant);
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		UserInterface::parseInt(args, "msbIn", &msbIn);
		UserInterface::parseBoolean(args, "signedInput", &signedInput);
		UserInterface::parseFloat(args, "targetUlpError", &targetUlpError);	
		return new FixRealShiftAdd(
													parentOp,
													target, 
													signedInput,
													msbIn,
													lsbIn,
													lsbOut,
													constant, 
													targetUlpError
													);
	}


}//namespace
#endif // defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)

namespace flopoco {
	void flopoco::FixRealShiftAdd::registerFactory()
	{
#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)
		UserInterface::add(
				"FixRealShiftAdd",
				"Table based real multiplier. Output size is computed",
				"ConstMultDiv",
				"",
				"signedInput(bool): 0=unsigned, 1=signed; \
				msbIn(int): weight associated to most significant bit (including sign bit);\
				lsbIn(int): weight associated to least significant bit;\
				lsbOut(int): weight associated to output least significant bit; \
				constant(string): constant given in arbitrary-precision decimal, or as a Sollya expression, e.g \"log(2)\"; \
				targetUlpError(real)=1.0: required precision on last bit. Should be strictly greater than 0.5 and lesser than 1;",
				"This variant of Ken Chapman's Multiplier is briefly described in <a href=\"bib/flopoco.html#DinIstoMas2014-SOPCJR\">this article</a>.<br> Special constants, such as 0 or powers of two, are handled efficiently.",
				FixRealShiftAdd::parseArguments,
				FixRealShiftAdd::unitTest
		);
#endif //#defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)
	}
}//namespace




