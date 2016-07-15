/*
  Euclidean division by a small constant

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

*/

// TODOs: remove from d its powers of two .

#include <iostream>
#include <fstream>
#include <sstream>

#include "IntConstDiv.hpp"


using namespace std;


namespace flopoco{


	// The classical table for the linear architecture
	
	IntConstDiv::EuclideanDivTable::EuclideanDivTable(Target* target, int d_, int alpha_, int gamma_) :
		Table(target, alpha_+gamma_, alpha_+gamma_, 0, -1, 1), d(d_), alpha(alpha_), gamma(gamma_) {
				ostringstream name;
				srcFileName="IntConstDiv::EuclideanDivTable";
				name <<"EuclideanDivTable_" << d << "_" << alpha ;
				setNameWithFreqAndUID(name.str());
	};

	mpz_class IntConstDiv::EuclideanDivTable::function(int x){
		// machine integer arithmetic should be safe here
		if (x<0 || x>=(1<<(alpha+gamma))){
			ostringstream e;
			e << "ERROR in IntConstDiv::EuclideanDivTable::function, argument out of range" <<endl;
			throw e.str();
		}

		int q = x/d;
		int r = x%d;
		return mpz_class((q<<gamma) + r);
	};


	
	IntConstDiv::CBLKTable::CBLKTable(Target* target, int level, int d, int alpha, int gamma, int rho):
		// Sizes below assume alpha>=gamma
		Table(target, (level==0? alpha: 2*gamma), (level==0? rho+gamma: (1<<level)*alpha-alpha+rho+gamma), 0, -1, 1), level(level), d(d), alpha(alpha), gamma(gamma), rho(rho) {
				ostringstream name;
				srcFileName="IntConstDiv::CLBKTable";
				name <<"CLBKTable_" << d << "_" << alpha << "_l" << level ;
				setNameWithFreqAndUID(name.str());
	}
	
	mpz_class IntConstDiv::CBLKTable::function(int x){
		if(level==0){ // the input is one radix-2^alpha digit 
			mpz_class r = x % d;
			mpz_class q = x / d;
			return (q<<gamma) + r;
		}
		else{ // the input consists of two remainders
			int r0 = x & ((1<<gamma)-1);
			int r1 = x >> gamma;
			mpz_class y = mpz_class(r0) + (mpz_class(r1) << alpha*((1<<(level-1))) );
			mpz_class r = y % d;
			mpz_class q = y / d;
			// cout << getName() << "   x=" << x << "  r0=" << r0 << " r1=" << r1 << "  y=" << y << " r=" << r << "  q=" << q << endl;
			return (q<<gamma) + r;
		}
	}

	
	int IntConstDiv::quotientSize() {return qSize; };

	int IntConstDiv::remainderSize() {return gamma; };




	IntConstDiv::IntConstDiv(Target* target, int wIn_, int d_, int alpha_, int architecture_,  bool remainderOnly_, map<string, double> inputDelays)
		: Operator(target), d(d_), wIn(wIn_), alpha(alpha_), architecture(architecture_), remainderOnly(remainderOnly_)
	{
		setCopyrightString("F. de Dinechin (2011)");
		srcFileName="IntConstDiv";

		gamma = intlog2(d-1);

		if(gamma>4) {
			REPORT(LIST, "WARNING: This operator is efficient for small constants. " << d << " is quite large. Attempting anyway.");
		}
		if(alpha==-1){
			if(architecture==0) {
				alpha = target->lutInputs()-gamma;
				if (alpha<1) {
					REPORT(LIST, "WARNING: This value of d is too large for the LUTs of this FPGA (alpha="<<alpha<<").");
					REPORT(LIST, " Building an architecture nevertheless, but it may be very large and inefficient.");
					alpha=1;
				}
			}
			else {
#if 0 // DEBUG IN HEXA
				alpha=4;
#else
				alpha = target->lutInputs();
#endif				
			}
		}
		qSize = intlog2(  ((mpz_class(1)<<wIn)-1)/d  );
		rho = intlog2(  ((mpz_class(1)<<alpha)-1)/d  );

		REPORT(INFO, "alpha="<<alpha);
		REPORT(DEBUG, "gamma=" << gamma << " qSize=" << qSize << " rho=" << rho);

		if((d&1)==0)
			REPORT(LIST, "WARNING, d=" << d << " is even, this is suspiscious. Might work nevertheless, but surely suboptimal.")

		/* Generate unique name */

		std::ostringstream o;
		if(remainderOnly)
			o << "IntConstRem_";
		else
			o << "IntConstDiv_";
		o << d << "_" << wIn << "_"  << alpha ;

		setNameWithFreqAndUID(o.str());


		addInput("X", wIn);


		if(!remainderOnly)
			addOutput("Q", qSize);
		addOutput("R", gamma);

		int nbDigitsIn = wIn/alpha;
		int inPadBits = wIn-nbDigitsIn*alpha;
		if (inPadBits!=0) nbDigitsIn++;

		
		REPORT(INFO, "Architecture splits the input in nbDigitsIn=" << nbDigitsIn  <<  " chunks."   );
		REPORT(DEBUG, "  d=" << d << "  wIn=" << wIn << "  alpha=" << alpha << "  gamma=" << gamma <<  "  nbDigitsIn=" << nbDigitsIn  <<  "  qSize=" << qSize );

		if(architecture==0) {
			//////////////////////////////////////// Linear architecture //////////////////////////////////:
			EuclideanDivTable* table;
			table = new EuclideanDivTable(target, d, alpha, gamma);
			useSoftRAM(table);
			addSubComponent(table);
			double tableDelay=table->getOutputDelay("Y");
			
			string ri, xi, ini, outi, qi;
			ri = join("r", nbDigitsIn);
			vhdl << tab << declare(ri, gamma) << " <= " << zg(gamma, 0) << ";" << endl;

			setCriticalPath( getMaxInputDelays(inputDelays) );


			for (int i=nbDigitsIn-1; i>=0; i--) {

				manageCriticalPath(tableDelay);

				//			cerr << i << endl;
				xi = join("x", i);
				if(i==nbDigitsIn-1 && inPadBits!=0) // at the MSB, pad with 0es
					vhdl << tab << declare(xi, alpha, true) << " <= " << zg(alpha-inPadBits, 0) <<  " & X" << range(wIn-1, i*alpha) << ";" << endl;
				else // normal case
					vhdl << tab << declare(xi, alpha, true) << " <= " << "X" << range((i+1)*alpha-1, i*alpha) << ";" << endl;
				ini = join("in", i);
				vhdl << tab << declare(ini, alpha+gamma) << " <= " << ri << " & " << xi << ";" << endl; // This ri is r_{i+1}
				outi = join("out", i);
				outPortMap(table, "Y", outi);
				inPortMap(table, "X", ini);


				vhdl << instance(table, join("table",i));
				ri = join("r", i);
				qi = join("q", i);
				vhdl << tab << declare(qi, alpha, true) << " <= " << outi << range(alpha+gamma-1, gamma) << ";" << endl;
				vhdl << tab << declare(ri, gamma) << " <= " << outi << range(gamma-1, 0) << ";" << endl;
			}


			if(!remainderOnly) { // build the quotient output
				vhdl << tab << declare("tempQ", nbDigitsIn*alpha) << " <= " ;
				for (unsigned int i=nbDigitsIn-1; i>=1; i--)
					vhdl << "q" << i << " & ";
				vhdl << "q0 ;" << endl;
				vhdl << tab << "Q <= tempQ" << range(qSize-1, 0)  << ";" << endl;
			}

			vhdl << tab << "R <= " << ri << ";" << endl; // This ri is r_0
		}



		// Bug on  ./flopoco IntConstDiv alpha=6 arch=1 d=3 remainderOnly=false wIn=31 TestBench n=1000
		// because qisize=0 below. The input is split into 6 chunks but Q fits on 30 bits and needs 5 chunks only
		else if (architecture==1){
			//////////////////////////////////////// Logarithmic architecture //////////////////////////////////:
			// The management of the tree is quite intricate when everything is not a power of two.
			
			// The number of levels is computed out of the number of digits of the _input_
			int levels = intlog2(2*nbDigitsIn-1); 
			REPORT(INFO, "levels=" << levels);
			CBLKTable* table;
			string ri, xi, ini, outi, qi, qs, r;

			// level 0
			table = new CBLKTable(target, 0, d, alpha, gamma, rho);
			useSoftRAM(table);
			addSubComponent(table);
			//			double tableDelay=table->getOutputDelay("Y");
			for (int i=0; i<nbDigitsIn; i++) {
				xi = join("x", i);
				if(i==nbDigitsIn-1 && inPadBits!=0) // at the MSB, pad with 0es
					vhdl << tab << declare(xi, alpha, true) << " <= " << zg(alpha-inPadBits, 0) <<  " & X" << range(wIn-1, i*alpha) << ";" << endl;
				else // normal case
					vhdl << tab << declare(xi, alpha, true) << " <= " << "X" << range((i+1)*alpha-1, i*alpha) << ";" << endl;
				outi = join("out", i);

				outPortMap(table, "Y", outi);
				inPortMap(table, "X", xi);
				vhdl << instance(table, join("table",i));
				ri = join("r_l0_", i);
				qi = join("qs_l0_", i);
				// The qi out of the table are on rho bits, and we want to pad them to alpha bits
				int qiSize;
				if(i<nbDigitsIn-1) {
						qiSize = alpha;
						vhdl << tab << declare(qi, qiSize, true) << " <= " << zg(qiSize -rho) << " & (" <<outi << range(rho+gamma-1, gamma) << ");" << endl;
					}
					else {
						qiSize = qSize - (nbDigitsIn-1)*alpha;
						REPORT(INFO, "-- qsize=" << qSize << " qisize=" << qiSize << "   rho=" << rho);
						if(qiSize>=rho)
							vhdl << tab << declare(qi, qiSize, true) << " <= " << zg(qiSize -rho) << " & (" <<outi << range(rho+gamma-1, gamma) << ");" << endl;
						else
							vhdl << tab << declare(qi, qiSize, true) << " <= " << outi << range(qiSize+gamma-1, gamma) << ";" << endl;
					} 
				vhdl << tab << declare(ri, gamma) << " <= " << outi << range(gamma-1, 0) << ";" << endl;
			}

			bool previousLevelOdd = ((nbDigitsIn&1)==1);
			// The following levels
			for (int level=1; level<levels; level++){
				int levelSize = nbDigitsIn/(1<<level); // how many sub-quotients we have in this level
				if (nbDigitsIn%((1<<level)) !=0 )
					levelSize++;
				REPORT(INFO, "level=" << level << "  levelSize=" << levelSize);
				table = new CBLKTable(target, level, d, alpha, gamma, rho);
				useSoftRAM(table);
				addSubComponent(table);
				for(int i=0; i<levelSize; i++) {
					string tableNumber = "l" + to_string(level) + "_" + to_string(i);
					string tableName = "table_" + tableNumber;
					string in = "in_" + tableNumber;
					string out = "out_" + tableNumber;
				  r = "r_"+ tableNumber;
					string q = "q_"+ tableNumber;
					if(i==levelSize-1 && previousLevelOdd) 
						vhdl << tab << declare(in, 2*gamma) << " <= " << zg(gamma) << " & r_l" << level-1 << "_" << 2*i  << ";"  << endl;
					else
						vhdl << tab << declare(in, 2*gamma) << " <= " << "r_l" << level-1 << "_" << 2*i+1 << " & r_l" << level-1 << "_" << 2*i  << ";"  << endl;

						
					outPortMap(table, "Y", out);
					inPortMap(table, "X", in);
					vhdl << instance(table, "table_"+ tableNumber);

					vhdl << tab << declare(r, gamma) << " <= " << out << range (gamma-1, 0) << ";"  << endl;

					int subQSize; // The size, in bits, of the part of Q we are building
					if (i<levelSize-1) {
						subQSize = (1<<level)*alpha;
						vhdl << tab << declare(q, subQSize) << " <= " << zg(subQSize - (table->wOut-gamma)) << " & " << out << range (table->wOut-1, gamma) << ";"  << endl;
					}
					else { // leftmost chunk
						subQSize = qSize-(levelSize-1)*(1<<level)*alpha;
						vhdl << tab << declare(q, subQSize) << " <= " ;
						if(subQSize >= (table->wOut-gamma))
							vhdl << zg(subQSize - (table->wOut-gamma)) << " & " << out << range (table->wOut-1, gamma) << ";"  << endl;
						else
							vhdl << out << range (subQSize+gamma-1, gamma) << ";"  << endl;
							
					}
					REPORT(INFO, "level=" << level << "  i=" << i << "  subQSize=" << subQSize << "  wOut=" << table->wOut << " gamma=" << gamma );

					
					qs = "qs_"+ tableNumber;
					string qsl =  "qs_l" + to_string(level-1) + "_" + to_string(2*i+1);
					string qsr =  "qs_l" + to_string(level-1) + "_" + to_string(2*i);
					vhdl << tab << declare(qs, subQSize) << " <= " ;
					if((i==levelSize-1) && (subQSize <= (1<<(level-1))*alpha) )
						vhdl << q << " + " << qsr << ";  -- partial quotient so far"  << endl;
					else
						vhdl << q << " + (" <<  qsl << " & " << qsr << ");  -- partial quotient so far"  << endl;
						
					}
				previousLevelOdd = ((levelSize&1)==1);
				
			}

			if(!remainderOnly) { // build the quotient output
				vhdl << tab << "Q <= " << qs  << range(qSize-1, 0) << ";" << endl;
			}

			vhdl << tab << "R <= " << r << ";" << endl;


		}

		
		else{
			THROWERROR("arch=" << architecture << " not supported");
		}
		
	}

	IntConstDiv::~IntConstDiv()
	{
	}





	
	void IntConstDiv::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class X = tc->getInputValue("X");
		/* Compute correct value */
		mpz_class Q = X/d;
		mpz_class R = X%d;
		if(!remainderOnly)
			tc->addExpectedOutput("Q", Q);
		tc->addExpectedOutput("R", R);
	}



	void IntConstDiv::nextTestState(TestState * previousTestState)
	{
		// the static list of mandatory tests
		static vector<vector<pair<string,string>>> testStateList;
		vector<pair<string,string>> paramList;
		
		// is initialized here
		if(previousTestState->getIterationIndex() == 0)		{

			for(int wIn=8; wIn<17; wIn+=1) { // test various input widths
				for(int d=3; d<=17; d+=2) { // test various divisors
					for(int arch=1; arch <2; arch++) { // test various architectures // TODO FIXME TO TEST THE LINEAR ARCH, TOO
						paramList.push_back(make_pair("wIn", to_string(wIn) ));	
						paramList.push_back(make_pair("d", to_string(d) ));	
						paramList.push_back(make_pair("arch", to_string(arch) ));	
						testStateList.push_back(paramList);
						paramList.clear();
					}
				}
			}
			previousTestState->setIterationNumber(testStateList.size());
		}

		// Now actually change the state
		vector<pair<string,string>>::iterator itVector;
		int testIndex = previousTestState->getIterationIndex();

		for(itVector = testStateList[testIndex].begin(); itVector != testStateList[testIndex].end(); ++itVector)
		{
			previousTestState->changeValue((*itVector).first,(*itVector).second);
		}
	}


	
	OperatorPtr IntConstDiv::parseArguments(Target *target, vector<string> &args) {
		int wIn, d, arch, alpha;
		bool remainderOnly;
		UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn); 
		UserInterface::parseStrictlyPositiveInt(args, "d", &d);
		UserInterface::parseInt(args, "alpha", &alpha);
		UserInterface::parsePositiveInt(args, "arch", &arch);
		UserInterface::parseBoolean(args, "remainderOnly", &remainderOnly);
		return new IntConstDiv(target, wIn, d, alpha, arch, remainderOnly);
	}

	void IntConstDiv::registerFactory(){
		UserInterface::add("IntConstDiv", // name
											 "Integer divider by a small constant.",
											 "ConstMultDiv",
											 "", // seeAlso
											 "wIn(int): input size in bits; \
                        d(int): small integer to divide by;  \
                        arch(int)=0: architecture used -- 0 for linear-time, 1 for log-time; \
                        remainderOnly(bool)=false: if true, the architecture doesn't output the quotient; \
                        alpha(int)=-1: Algorithm uses radix 2^alpha. -1 choses a sensible default.",
											 "This operator is described in <a href=\"bib/flopoco.html#dedinechin:2012:ensl-00642145:1\">this article</a>.",
											 IntConstDiv::parseArguments,
											 IntConstDiv::nextTestState
											 ) ;
	}

	
}
