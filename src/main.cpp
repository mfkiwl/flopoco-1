/*
 * The FloPoCo command-line interface
 *
 * Author : Florent de Dinechin
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

#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <mpfr.h>

#include "Operator.hpp"
#include "Target.hpp"
#include "Targets/VirtexIV.hpp"
#include "Targets/StratixII.hpp"
#include "Shifters.hpp"
#include "LZOC.hpp"
#include "LZOCShifterSticky.hpp"
#include "IntAdder.hpp"
#include "IntMultiplier.hpp"
#include "Karatsuba.hpp"
#include "FPMultiplier.hpp"
#include "LongAcc.hpp"
#include "LongAcc2FP.hpp"
#include "FPAdder.hpp"
#include "DotProduct.hpp"
#include "Wrapper.hpp"
#include "TestBench.hpp"
#include "BigTestBench.hpp"
#include "ConstMult/IntConstMult.hpp"
#include "ConstMult/FPConstMult.hpp"
#include "ConstMult/CRFPConstMult.hpp"
#include "FPExp.hpp"
#include "FPLog.hpp"
#ifdef HAVE_HOTBM
#include "HOTBM.hpp"
#endif


using namespace std;


// Global variables, useful through most of FloPoCo

vector<Operator*> oplist;

string filename="flopoco.vhdl";
string cl_name=""; // used for the -name option

int verbose=0;
Target* target;

static void usage(char *name){
	cerr << "\nUsage: "<<name<<" <operator specification list>\n" ;
	cerr << "Each operator specification is one of: \n";
	cerr << "    LeftShifter  wIn  MaxShift\n";
	cerr << "    RightShifter wIn  MaxShift\n";
	cerr << "    LZOC wIn wOut\n";
	cerr << "    LZOCShifterSticky wIn wOut computeSticky countType\n";
	cerr << "      CountType=0|1 fixed counting. -1 generic Counting (extra entity port)\n";
	//	cerr << "    Mux wIn n \n"; killed by Florent
	cerr << "    IntAdder wIn\n";
	cerr << "      Integer adder, possibly pipelined\n";
	cerr << "    FPAdder wEX wFX wEY wFY wER wFR\n";
	cerr << "      Floating-point adder \n";
	cerr << "    IntMultiplier wInX wInY \n";
	cerr << "      Integer multiplier of two integers X and Y of sizes wInX and wInY \n";	
	// not ready for release
	//	cerr << "    Karatsuba wInX wInY \n";
	//	cerr << "      integer multiplier of two integers X and Y of sizes wInX and wInY. For now the sizes must be equal \n";	
	cerr << "    FPMultiplier wEX wFX wEY wFY wER wFR\n";
	cerr << "      Floating-point multiplier \n";
	cerr << "    IntConstMult w c\n";
	cerr << "      Integer constant multiplier: w - input size, c - the constant\n";
	cerr << "    FPConstMult wE_in wF_in wE_out wF_out cst_sgn cst_exp cst_int_sig\n";
	cerr << "      Floating-point constant multiplier (NPY)\n";
	cerr << "      The constant is provided as integral significand and integral exponent.\n";
#ifdef HAVE_SOLLYA
	cerr << "    CRFPConstMult  wE_in wF_in  wE_out wF_out  constant_expr \n";
	cerr << "      Correctly-rounded floating-point constant multiplier (NPY)\n";
	cerr << "      The constant is provided as a Sollya expression, between double quotes.\n";
#endif // HAVE_SOLLYA
	cerr << "    LongAcc wE_in wF_in MaxMSB_in LSB_acc MSB_acc\n";
	cerr << "      Long fixed-point accumulator\n";
	cerr << "    LongAcc2FP LSB_acc MSB_acc wE_out wF_out \n";
	cerr << "      Post-normalisation unit for LongAcc \n";
	cerr << "    DotProduct wE wFX wFY MaxMSB_in LSB_acc MSB_acc\n";
	cerr << "      Floating-point dot product unit \n";
	cerr << "    FPExp wE wF\n";
	cerr << "      Floating-point exponential function (NPY)\n";
	cerr << "    FPLog wE wF\n";
	cerr << "      Floating-point logarithm function (NPY)\n";
#ifdef HAVE_HOTBM
	cerr << "    HOTBM function wI wO degree\n";
	cerr << "      High-Order Table-Based Method for fixed-point functions (NPY)\n";
	cerr << "      wI - input width, wO - output width, degree - degree of polynomial approx\n";
	cerr << "      function - sollya-syntaxed function to implement, between double quotes\n";
#endif // HAVE_HOTBM
	cerr << "    TestBench n\n";
	cerr << "       Behavorial test bench for the preceding operator\n";
	cerr << "       This test bench will include standard tests, plus n random tests.\n";
	cerr << "    BigTestBench n\n";
	cerr << "       Same as above, more VHDL efficient, less practical for debugging.\n";
	cerr << "    Wrapper\n";
	cerr << "       Wraps the preceding operator between registers\n";
	cerr << "(NPY) Not pipelined yet\n";
	cerr << "General options, affecting the operators that follow them:\n";
	cerr << "   -outputfile=<output file name>    (default=flopoco.vhdl)\n";
	cerr << "   -verbose=<1|2|3>                  (default=0)\n";
	cerr << "   -pipeline=<yes|no>                (default=yes)\n";
	cerr << "   -frequency=<frequency in MHz>     (default=400)\n";
	cerr << "   -target=<StratixII|VirtexIV>      (default=VirtexIV)\n";
	cerr << "   -DSP_blocks=<yes|no>\n";
	cerr << "       optimize for the use of DSP blocks (default=yes)\n";
	cerr << "   -name=<entity name>\n";
	cerr << "       defines the name of the VHDL entity of the next operator\n";
	exit (EXIT_FAILURE);
}

int checkStrictyPositive(char* s, char* cmd) {
	int n=atoi(s);
	if (n<=0){
		cerr<<"ERROR: got "<<s<<", expected strictly positive number."<<endl;
		usage(cmd);
	}
	return n;
}

int checkPositiveOrNull(char* s, char* cmd) {
	int n=atoi(s);
	if (n<0){
		cerr<<"ERROR: got "<<s<<", expected positive-or-null number."<<endl;
		usage(cmd);
	}
	return n;
}

bool checkBoolean(char* s, char* cmd) {
	int n=atoi(s);
	if (n!=0 && n!=1) {
		cerr<<"ERROR: got "<<s<<", expected a boolean (0 or 1)."<<endl;
		usage(cmd);
	}
	return (n==1);
}


int checkSign(char* s, char* cmd) {
	int n=atoi(s);
	if (n!=0 && n!=1) {
		cerr<<"ERROR: got "<<s<<", expected a sign bit (0 or 1)."<<endl;
		usage(cmd);
	}
	return n;
}


void addOperator(Operator *op) {
	if(cl_name!="")	{
		op->setCommentedName(op->getOperatorName());
		op->setOperatorName(cl_name);
		cl_name="";
	}
	// TODO check name not already in list...
	// TODO this procedure should be a static method of operator, so that other operators can call it
	oplist.push_back(op);
}



bool parseCommandLine(int argc, char* argv[]){
	if (argc<2) {
		usage(argv[0]); 
		return false;
	}

	Operator* op;
	int i=1;
	cl_name="";
	do {
		string opname(argv[i++]);
		if(opname[0]=='-'){
			string::size_type p = opname.find('=');
			if (p == string::npos) {
				cerr << "ERROR: Option missing an = : "<<opname<<endl; 
				return false;
			} else {
				string o = opname.substr(1, p - 1), v = opname.substr(p + 1);
				if (o == "outputfile") {
					if(oplist.size()!=0)
						cerr << "WARNING: global option "<<o<<" should come before any operator specification" << endl; 
					filename=v;
				}
				else if (o == "verbose") {
					verbose = atoi(v.c_str()); // there must be a more direct method of string
					if (verbose<0 || verbose>3) {
						cerr<<"ERROR: verbose should be 1, 2 or 3,    got "<<v<<"."<<endl;
						usage(argv[0]);
					}
				}
				else if (o == "target") {
					if(v=="VirtexIV") target=new VirtexIV();
					else if (v=="StratixII") target=new StratixII();
					else {
						cerr<<"ERROR: unknown target: "<<v<<endl;
						usage(argv[0]);
					}
				}
				else if (o == "pipeline") {
					if(v=="yes") target->setPipelined();
					else if(v=="no")  target->setNotPipelined();
					else {
						cerr<<"ERROR: pipeline option should be yes or no,    got "<<v<<"."<<endl; 
						usage(argv[0]);
					}
				}
				else if (o == "frequency") {
					int freq = atoi(v.c_str());
					if (freq>1 && freq<10000) {
						target->setFrequency(1e6*(double)freq);
						if(verbose) 
							cerr << "Frequency set to "<<target->frequency()<< " Hz" <<endl; 
					}
					else {
						cerr<<"WARNING: frequency out of reasonible range, ignoring it."<<endl; 
					}
				}
				else if (o == "DSP_blocks") {
					if(v=="yes") target->setUseHardMultipliers(true);
					else if(v=="no")  target->setUseHardMultipliers(false);
					else {
						cerr<<"ERROR: DSP_blocks option should be yes or no,    got "<<v<<"."<<endl; 
						usage(argv[0]);
					}
				}
				else if (o == "name") {
					cl_name=v; // TODO?  check it is a valid VHDL entity name 
				}
				else { 	
					cerr << "Unknown option "<<o<<endl; 
					return false;
				}
			}
		}
		else if(opname=="IntConstMult"){
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int w = atoi(argv[i++]);
				mpz_class mpc(argv[i++]);
				cerr << "> IntConstMult , w="<<w<<", c="<<mpc<<"\n";
				op = new IntConstMult(target, w, mpc);
				addOperator(op);
			}        
		}
		else if(opname=="FPConstMult"){
			int nargs = 7;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wE_in = checkStrictyPositive(argv[i++], argv[0]);
				int wF_in = checkStrictyPositive(argv[i++], argv[0]);
				int wE_out = checkStrictyPositive(argv[i++], argv[0]);
				int wF_out = checkStrictyPositive(argv[i++], argv[0]);
				int cst_sgn  = checkSign(argv[i++], argv[0]); 
				int cst_exp  = atoi(argv[i++]); // TODO no check on this arg
				mpz_class cst_sig(argv[i++]);
				cerr << "> FPConstMult, wE_in="<<wE_in<<", wF_in="<<wF_in
						 <<", wE_out="<<wE_out<<", wF_out="<<wF_out
						 <<", cst_sgn="<<cst_sgn<<", cst_exp="<<cst_exp<< ", cst_sig="<<cst_sig<<endl;
				op = new FPConstMult(target, wE_in, wF_in, wE_out, wF_out, cst_sgn, cst_exp, cst_sig);
				addOperator(op);
			}        
		} 	
#ifdef HAVE_SOLLYA
		else if(opname=="CRFPConstMult"){
			int nargs = 5;
			if (i+nargs > argc)
				usage(argv[0]);
			else { 
				int wE_in = checkStrictyPositive(argv[i++], argv[0]);
				int wF_in = checkStrictyPositive(argv[i++], argv[0]);
				int wE_out = checkStrictyPositive(argv[i++], argv[0]);
				int wF_out = checkStrictyPositive(argv[i++], argv[0]);
				string constant = argv[i++];
				cerr << "> CRFPConstMult, wE_in="<<wE_in<<", wF_in="<<wF_in 
					  <<", wE_out="<<wE_out<<", wF_out="<<wF_out
					  << ", constant="<<constant <<endl;
				op = new CRFPConstMult(target, wE_in, wF_in, wE_out, wF_out, constant);
				addOperator(op);
			}        
		} 	
#endif // HAVE_SOLLYA
		else if(opname=="LeftShifter"){
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wIn = checkStrictyPositive(argv[i++], argv[0]);
				int maxShift = checkStrictyPositive(argv[i++], argv[0]);
				cerr << "> LeftShifter, wIn="<<wIn<<", maxShift="<<maxShift<<"\n";
				op = new Shifter(target, wIn, maxShift, Left);
				addOperator(op);
			}    
		}
		else if(opname=="RightShifter"){
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wIn = checkStrictyPositive(argv[i++], argv[0]);
				int maxShift = checkStrictyPositive(argv[i++], argv[0]);
				cerr << "> RightShifter, wIn="<<wIn<<", maxShift="<<maxShift<<"\n";
				op = new Shifter(target, wIn, maxShift, Right);
				addOperator(op);
			}
		}
		else if(opname=="LZOC"){
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wIn = checkStrictyPositive(argv[i++], argv[0]);
				int wOut = checkStrictyPositive(argv[i++], argv[0]);
				cerr << "> LZOC, wIn="<<wIn<<", wOut="<<wOut<<"\n";
				op = new LZOC(target, wIn, wOut);
				addOperator(op);
			}
		}
		else if(opname=="LZOCShifterSticky"){
			int nargs = 4;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wIn = checkStrictyPositive(argv[i++], argv[0]);
				int wOut = checkStrictyPositive(argv[i++], argv[0]);
				bool computesticky  = checkBoolean(argv[i++], argv[0]);
				int countWhat = atoi(argv[i++]);
				cerr << "> LZOCShifterSticky, wIn="<<wIn<<", wOut="<<wOut<<", compute_sticky=" << computesticky<<", zeroOrOne="<<countWhat<< endl;
				op = new LZOCShifterSticky(target, wIn, wOut, computesticky, countWhat);
				addOperator(op);
			}
		}
		else if(opname=="IntAdder"){
			int nargs = 1;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wIn = checkStrictyPositive(argv[i++], argv[0]);
				cerr << "> IntAdder, wIn="<<wIn<<endl  ;
				op = new IntAdder(target, wIn);
				addOperator(op);
			}    
		}   
		else if(opname=="IntMultiplier"){
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wInX = checkStrictyPositive(argv[i++], argv[0]);
				int wInY = checkStrictyPositive(argv[i++], argv[0]);
				cerr << "> IntMultiplier , wInX="<<wInX<<", wInY="<<wInY<<"\n";
				op = new IntMultiplier(target, wInX, wInY);
				addOperator(op);
			}
		}
		else if(opname=="Karatsuba"){
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wInX = checkStrictyPositive(argv[i++], argv[0]);
				int wInY = checkStrictyPositive(argv[i++], argv[0]);
				cerr << "> Karatsuba , wInX="<<wInX<<", wInY="<<wInY<<"\n";
				op = new Karatsuba(target, wInX, wInY);
				addOperator(op);
			}    
		}   
		else if(opname=="FPMultiplier"){
			int nargs = 6; 
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wEX = checkStrictyPositive(argv[i++], argv[0]);
				int wFX = checkStrictyPositive(argv[i++], argv[0]);
				int wEY = checkStrictyPositive(argv[i++], argv[0]);
				int wFY = checkStrictyPositive(argv[i++], argv[0]);
				int wER = checkStrictyPositive(argv[i++], argv[0]);
				int wFR = checkStrictyPositive(argv[i++], argv[0]);

				cerr << "> FPMultiplier , wEX="<<wEX<<", wFX="<<wFX<<", wEY="<<wEY<<", wFY="<<wFY<<", wER="<<wER<<", wFR="<<wFR<<" \n";
				
				if ((wEX==wEY) && (wEX==wER)){
					op = new FPMultiplier(target, wEX, wFX, wEY, wFY, wER, wFR, 1);
					addOperator(op);
				}else
					cerr<<"(For now) the inputs must have the same size"<<endl;
			}
		} 
		else if(opname=="FPAdder"){
			int nargs = 6;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wEX = checkStrictyPositive(argv[i++], argv[0]);
				int wFX = checkStrictyPositive(argv[i++], argv[0]);
				int wEY = checkStrictyPositive(argv[i++], argv[0]);
				int wFY = checkStrictyPositive(argv[i++], argv[0]);
				int wER = checkStrictyPositive(argv[i++], argv[0]);
				int wFR = checkStrictyPositive(argv[i++], argv[0]);
				
				cerr << "> FPAdder , wEX="<<wEX<<", wFX="<<wFX<<", wEY="<<wEY<<", wFY="<<wFY<<", wER="<<wER<<", wFR="<<wFR<<" \n";
						
					if ((wEX==wEY) && (wEX==wER)){
						op = new FPAdder(target, wEX, wFX, wEY, wFY, wER, wFR);
						addOperator(op);
					}else
						cerr<<"(For now) the inputs and outputs must have the same size"<<endl;
			}
		} 
		else if(opname=="LongAcc"){
			int nargs = 5;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wEX = checkStrictyPositive(argv[i++], argv[0]);
				int wFX = checkStrictyPositive(argv[i++], argv[0]);
				int MaxMSBX = atoi(argv[i++]); // may be negative
				int LSBA = atoi(argv[i++]); // may be negative
				int MSBA = atoi(argv[i++]); // may be negative
				cerr << "> Long accumulator , wEX="<<wEX<<", wFX="<<wFX<<", MaxMSBX="<<MaxMSBX<<", LSBA="<<LSBA<<", MSBA="<<MSBA<<"\n";
				op = new LongAcc(target, wEX, wFX, MaxMSBX, LSBA, MSBA);
				addOperator(op);
			}
		}
		// hidden and undocumented
		else if(opname=="LongAccPrecTest"){
			int nargs = 6; // same as LongAcc, plus an iteration count
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wEX = atoi(argv[i++]);
				int wFX = atoi(argv[i++]);
				int MaxMSBX = atoi(argv[i++]);
				int LSBA = atoi(argv[i++]);
				int MSBA = atoi(argv[i++]);
				int n = atoi(argv[i++]);
				cerr << "> Test of long accumulator accuracy, wEX="<<wEX<<", wFX="<<wFX<<", MaxMSBX="<<MaxMSBX<<", LSBA="<<LSBA<<", MSBA="<<MSBA<<", "<< n << " tests\n";
				LongAcc * op = new LongAcc(target, wEX, wFX, MaxMSBX, LSBA, MSBA);
				// op->test_precision(n);
				op->test_precision2();
			}    
		}
		else if(opname=="LongAcc2FP"){
			int nargs = 4;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int LSBA = atoi(argv[i++]); // may be negative
				int MSBA = atoi(argv[i++]); // may be negative
				int wE_out = checkStrictyPositive(argv[i++], argv[0]);
				int wF_out = checkStrictyPositive(argv[i++], argv[0]);
				cerr << "> Post-Normalization unit for Long accumulator, LSBA="<<LSBA<<", MSBA="<<MSBA<<" wE_out="<<wE_out<<", wF_out="<<wF_out<<"\n";
				op = new LongAcc2FP(target, LSBA, MSBA, wE_out, wF_out);
				addOperator(op);
			}
		}
		else if(opname=="DotProduct"){
			int nargs = 6;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				int wE = checkStrictyPositive(argv[i++], argv[0]);
				int wFX = checkStrictyPositive(argv[i++], argv[0]);
				int wFY = checkStrictyPositive(argv[i++], argv[0]);
				int MaxMSBX = atoi(argv[i++]); // may be negative
				int LSBA = atoi(argv[i++]); // may be negative
				int MSBA = atoi(argv[i++]); // may be negative
				cerr << "> DotProduct , wE="<<wE<<", wFX="<<wFX<<", wFY="<<wFY<<", MaxMSBX="<<MaxMSBX<<", LSBA="<<LSBA<<", MSBA="<<MSBA<<"\n";
				op = new DotProduct(target, wE, wFX, wFY, MaxMSBX, LSBA, MSBA);
				addOperator(op);
			}
		}
#ifdef HAVE_HOTBM
		else if (opname == "HOTBM") {
			int nargs = 4;
			if (i+nargs > argc)
				usage(argv[0]); // and exit
			string func = argv[i++];
			int wI = checkStrictyPositive(argv[i++], argv[0]);
			int wO = checkStrictyPositive(argv[i++], argv[0]);
			int n  = checkStrictyPositive(argv[i++], argv[0]);
			cerr << "> HOTBM func='" << func << "', wI=" << wI << ", wO=" << wO <<endl;
			op = new HOTBM(target, func, wI, wO, n);
			addOperator(op);
		}
#endif // HAVE_HOTBM
		else if (opname == "FPExp")
		{
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]); // and exit
			int wE = checkStrictyPositive(argv[i++], argv[0]);
			int wF = checkStrictyPositive(argv[i++], argv[0]);
			cerr << "> FPExp: wE=" << wE << " wF=" << wF << endl;
			op = new FPExp(target, wE, wF);
			addOperator(op);
		}
		else if (opname == "FPLog")
		{
			int nargs = 2;
			if (i+nargs > argc)
				usage(argv[0]); // and exit
			int wE = checkStrictyPositive(argv[i++], argv[0]);
			int wF = checkStrictyPositive(argv[i++], argv[0]);
			cerr << "> FPLog: wE=" << wE << " wF=" << wF << endl;
			op = new FPLog(target, wE, wF);
			addOperator(op);
		}
		else if (opname == "Wrapper") {
			int nargs = 0;
			if (i+nargs > argc)
				usage(argv[0]);
			else {
				if(oplist.empty()){
					cerr<<"ERROR: Wrapper has no operator to wrap (it should come after the operator it wraps)"<<endl;
					usage(argv[0]);
				}
				Operator* toWrap = oplist.back();
				cerr << "> Wrapper for " << toWrap->getOperatorName()<<endl;
				op =new Wrapper(target, toWrap);
				addOperator(op);
			}
		}
		else if (opname == "TestBench") {
			int nargs = 1;
			if (i+nargs > argc)
				usage(argv[0]); // and exit
			if(oplist.empty()){
				cerr<<"ERROR: TestBench has no operator to wrap (it should come after the operator it wraps)"<<endl;
				usage(argv[0]); // and exit
			}
			int n = checkPositiveOrNull(argv[i++], argv[0]);
			Operator* toWrap = oplist.back();
			cerr << "> TestBench for " << toWrap->getOperatorName()<<endl;
			if(cl_name!="")	op->setOperatorName(cl_name);
			oplist.push_back(new TestBench(target, toWrap, n));
		}
		else if (opname == "BigTestBench") {
			int nargs = 1;
			if (i+nargs > argc)
				usage(argv[0]); // and exit
			if(oplist.empty()){
				cerr<<"ERROR: BigTestBench has no operator to wrap (it should come after the operator it wraps)"<<endl;
				usage(argv[0]); // and exit
			}
			int n = checkPositiveOrNull(argv[i++], argv[0]);
			Operator* toWrap = oplist.back();
			cerr << "> BigTestBench for " << toWrap->getOperatorName()<<endl;
			if(cl_name!="")	op->setOperatorName(cl_name);
			oplist.push_back(new BigTestBench(target, toWrap, n));
		}
		else  {
			cerr << "ERROR: Problem parsing input line, exiting";
			usage(argv[0]);
		}
	} while (i<argc);
	return true;
}

int main(int argc, char* argv[] )
{
	target = new VirtexIV();

	try {
		bool command_line_OK =  parseCommandLine(argc, argv);
	} catch (std::string s) {
		cerr << "Exception while parsing command line: " << s << endl;
		return 1;
	}

	if(oplist.empty()) {
		cerr << "Nothing to generate, exiting\n";
		exit(EXIT_FAILURE);
	}

	ofstream file;
	file.open(filename.c_str(), ios::out);
	cerr<< "Output file: " << filename <<endl;
	
	for(int i=0; i<oplist.size(); i++) {
		try {
			oplist[i]->outputVHDL(file);
		} catch (std::string s) {
			cerr << "Exception while generating '" << oplist[i]->getOperatorName() << "': " << s <<endl;
		}
	}
	file.close();
	
	cout << endl<<"Final report:"<<endl;
	for(int i=0; i<oplist.size(); i++) {
		oplist[i]->outputFinalReport();
	}
	return 0;
}
