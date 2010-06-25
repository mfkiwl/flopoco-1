/*
 * A signed integer multiplier for FloPoCo
 *
 * Authors : Bogdan Pasca
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

#include <typeinfo>
#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "utils.hpp"
#include "Operator.hpp"
#include "SignedIntMultiplier.hpp"
#include "IntCompressorTree.hpp"

using namespace std;

namespace flopoco{

	extern vector<Operator*> oplist;

	SignedIntMultiplier:: SignedIntMultiplier(Target* target, int wInX, int wInY, map<string, double> inputDelays) :
		Operator(target), wInX_(wInX), wInY_(wInY), wOut_(wInX + wInY){
 		srcFileName = "SignedIntMultiplier";
 
 		double target_freq = target->frequency();
		double maxDSPFrequency = int(floor(1.0/ (target->DSPMultiplierDelay() + 1.0e-10)));
		
		if (target_freq > maxDSPFrequency)
			target->setFrequency(maxDSPFrequency);
 
		ostringstream name;
		name <<"SignedIntMultiplier_"<<wInX_<<"_"<<wInY_;
		setName(name.str());
	
		setCopyrightString("Bogdan Pasca (2010)");
	
		addInput ("X", wInX_);
		addInput ("Y", wInY_);
		addOutput("R", wOut_); /* wOut_ = wInX_ + wInY_ */
	
	
		if ((target->getUseHardMultipliers()) && (target->getNumberOfDSPs()>0))
		{
			REPORT(DETAILED, "The target is: " << target->getID());
			if ((target->getID()=="Virtex4")||(target->getID()=="Spartan3")){
				int x, y, xS, yS, cOp1, cOp2;

				string op1("Y"), op2("X");
				int wOp1(wInY), wOp2(wInX);				

				target->getDSPWidths(xS, yS, true);
				target->getDSPWidths(x,  y,  false);
				 
				cOp1 = getChunkNumber( wOp1, x, xS);
				cOp2 = getChunkNumber( wOp2, y, yS);
				
				REPORT(DEBUG, "cOp1="<<cOp1<<" cOp2="<<cOp2<< " x="<<x<<" xS="<<xS<< " y="<<y<<" yS="<<yS);

				if (cOp1 < cOp2){
					/* swap */
					op1 = string("X"); op2 = string("Y");
					wOp1 = wInX; wOp2 = wInY;
					int tmp = cOp1; cOp1 = cOp2; cOp2 = tmp; 
				}
	
				if (cOp1 + cOp2 > 2) { 

					vhdl << tab << declare("sY", y*(cOp1-1)+yS) << " <= " << op1 << " & " << zg(x*(cOp1-1)+xS -wOp1,0) << ";" << endl;
					vhdl << tab << declare("sX", x*(cOp2-1)+xS) << " <= " << op2 << " & " << zg(y*(cOp2-1)+yS -wOp2,0) << ";" << endl; //pad to the right with 0 (xst)

					//SPLITTINGS
					for (int k=0; k < cOp2 ; k++)
						vhdl << tab << declare(join("x",k), xS) << " <= " << (k<cOp2-1?"\"0\" & ":"") << "sX" << range((k<cOp2-1?(k+1)*x:x*(cOp2-1)+xS)-1,k*x) << ";" << endl;
					for (int k=0; k < cOp1 ; k++)
						vhdl << tab << declare(join("y",k), yS) << " <= " << (k<cOp1-1?"\"0\" & ":"") << "sY" << range((k<cOp1-1?(k+1)*y:y*(cOp1-1)+yS)-1,k*y) << ";" << endl;

					setCriticalPath( getMaxInputDelays(inputDelays));

				
					//MULTIPLICATIONS WITH SOME ACCUMULATIONS
					for (int i=0; i < cOp2; i++){ 
						setCriticalPath ( getMaxInputDelays(inputDelays) );
						for (int j=0; j < cOp1; j++){ 
							if (j==0){ // @ the first the operation is only multiplication, not MAC
								manageCriticalPath(target->DSPMultiplierDelay());
								vhdl << tab << declare(join("px",i,"y",j), xS+yS) << " <= " << join("x",i) << " * " << join("y",j) << ";" << endl;
							}else{
								manageCriticalPath(target->DSPlocalWireDelay() + target->DSPAdderDelay());
								vhdl << tab << declare(join("tpx",i,"y",j),xS+yS) << " <= " << join("x",i) << " * " << join("y",j) << ";" << endl; 
								vhdl << tab << declare(join("px",i,"y",j), xS+yS) << " <= " << join("tpx",i,"y",j) << " + " 
								                                                            << join("px",i,"y",j-1) << range(xS+yS-1,y) << ";" << endl; //sign extend TODO
							}
//							if (!((j==cOp1-1) && (i<cOp2-1))) nextCycle();
						}
						if (i<cOp2-1) setCycle(0); //reset cycle
					}
					
					REPORT(DEBUG, "Delay value at DSP outputs " << getCriticalPath());
			
					//FORM THE INTERMEDIARY PRODUCTS
					for (int i=0; i < cOp2 ; i++){
						vhdl << tab << declare(join("sum",i), y*(cOp1-1) + xS + yS) << " <= "; 
						for (int j=cOp1-1;j>=0; j--)
							vhdl << join("px",i,"y",j) << (j<cOp1-1?range(x-1,0):"") << (j==0 ? ";" : " & ");
						vhdl << endl;
					}

					if (cOp2 > 1){
						manageCriticalPath(target->DSPinterconnectWireDelay()+target->localWireDelay());
						vhdl << tab << declare ("sum0Low", x) << " <= sum0" << range(x-1,0) << ";" << endl;
						REPORT( DEBUG, "The delay at compressor tree input is = " << getCriticalPath());
//						IntNAdder* add =  new IntNAdder(target, y*(cOp1-1)+yS +  x*(cOp2-1)+xS - x, cOp2);
						IntCompressorTree* add =  new IntCompressorTree(target, y*(cOp1-1)+yS +  x*(cOp2-1)+xS - x, cOp2, inDelayMap("X0",getCriticalPath()));
						oplist.push_back(add);
		
						/* prepare operands */
						for (int i=0; i < cOp2; i++){
							if (i==0){ 
								vhdl << tab << declare (join("addOp",i), (cOp2-1)*x + y*(cOp1-1) + xS + yS - x ) << " <= "
								            << rangeAssign( (cOp2-1-i)*x-1, 0, join("sum",i)+of(y*(cOp1-1) + xS + yS - 1 ))  << " & " 
								            << join("sum",i) << range(y*(cOp1-1) + xS + yS - 1,x) << ";" <<endl;
							}else if (i==1){ 
								vhdl << tab << declare (join("addOp",i),(cOp2-1)*x + y*(cOp1-1) + xS + yS - x) << " <= " 
								            << rangeAssign( (cOp2-1-i)*x-1, 0, join("sum",i)+of(y*(cOp1-1) + xS + yS - 1 ))  << ((cOp2-1-i)*x-1>=0?" & ":"") 
								            << join("sum",i) << ";" <<endl;
							}else if ((i > 1) && ( i!= cOp2-1)){ 
								vhdl << tab << declare (join("addOp",i),(cOp2-1)*x + y*(cOp1-1) + xS + yS - x) << " <= " 
								            << rangeAssign( (cOp2-1-i)*x-1, 0, join("sum",i)+of(y*(cOp1-1) + xS + yS - 1 ))  << ((cOp2-1-i)*x-1>=0?" & ":"") 
								            << join("sum",i)  << " & "
								            << zg( (i-1)*x, 0) << ";" <<endl;
							}else if (i == cOp2-1){
								vhdl << tab << declare (join("addOp",i),(cOp2-1)*x + y*(cOp1-1) + xS + yS - x) << " <= " 
								            << join("sum",i)  << " & "
								            << zg( (i-1)*x, 0) << ";" <<endl;
							}
						}//TODO compact ifs

						for (int i=0; i< cOp2; i++)
							inPortMap (add, join("X",i) , join("addOp",i));
		
//						inPortMapCst(add, "Cin", "'0'");
						outPortMap(add, "R", "addRes");
						vhdl << instance(add, "adder");

						syncCycleFromSignal("addRes");
						outDelayMap["R"] = (add->getOutDelayMap())["R"];

						if ( (y*(cOp1-1)+yS + x*(cOp2-1)+xS ) - (wInX + wInY) < x ){
							vhdl << tab << "R <= addRes & sum0Low "<<range(x-1, (y*(cOp1-1)+yS + x*(cOp2-1)+xS ) - (wInX + wInY) )<< ";" << endl;
						}else{	
							vhdl << tab << "R <= addRes"<<range(y*(cOp1-1)+yS +  x*(cOp2-1)+xS - x -1, (y*(cOp1-1)+yS + x*(cOp2-1)+xS -x ) - (wInX + wInY)	 )<< ";" << endl;
						}
					}else{
						vhdl << tab << "R <= sum0"<<range( y*(cOp1-1) + xS + yS - 1, y*(cOp1-1) + xS + yS - (wInX + wInY ) ) << ";" << endl;
						outDelayMap["R"] = getCriticalPath();
					}
				}else{
					setCriticalPath( getMaxInputDelays(inputDelays));
					manageCriticalPath( target->DSPMultiplierDelay());
					vhdl << tab << "R <= X * Y ;" <<endl;
				}
			}else if ((target->getID()=="Virtex5") || (target->getID()=="Virtex6")){
				//multiplier block size is 25x18 signed, 24x17 unsigned
				//check best splitting
				
				int l18[5],l25[5];
				l18[0]=18; 
				l18[1]=35;
				l18[2]=52;
				l18[3]=69;
				l18[4]=86;
				
				l25[0]=25;
				l25[1]=49;
				l25[2]=73;
				l25[3]=97;
				l25[4]=121;
				
				int k1_0=0, k2_0=0;
				while (l18[k1_0] < wInX)
					k1_0++;
				while (l25[k2_0] < wInY)
					k2_0++;
				
				int k1_1=0, k2_1=0;
				while (l18[k1_1] < wInY)
					k1_1++;
				while (l25[k2_1] < wInX)
					k2_1++;
				
				int cost0, cost1;
				cost0 = (k1_0+1)*(k2_0+1);
				cost1 = (k1_1+1)*(k2_1+1);
					
				REPORT(DEBUG, "First score is ="<<cost0);
				REPORT(DEBUG, "First score is ="<<cost1);
				
				string op1 = "X";
				string op2 = "Y";
				
				int wOp1, wOp2;
				int yS, xS;
				int cOp1, cOp2;

				/* we multiply A * B, a has to be split into chunks of 17 in order to use the internal adders */
				if (cost0 <= cost1){
					//already in best configuration: nothing to do
					wOp1 = wInX; 
					wOp2 = wInY;
					xS = l18[k1_0];
					yS = l25[k2_0];
					cOp1 = k1_0 + 1;
					cOp2 = k2_0 + 1;
				}else{
					op1 = "Y";
					op2 = "X";
					wOp1 = wInY; 
					wOp2 = wInX;
					xS = l18[k1_1];
					yS = l25[k2_1];
					cOp1 = k1_1 + 1;
					cOp2 = k2_1 + 1;
				}
				
				
				if (cOp1 + cOp2 > 2) { 
					/* pad the two inputs up to size */
					if (cost0 <= cost1){
						vhdl << tab << declare("sX", l18[k1_0]) << " <= " << op1 << " & " << zg(l18[k1_0] - wOp1,0) << ";" << endl;
						vhdl << tab << declare("sY", l25[k2_0]) << " <= " << op2 << " & " << zg(l25[k2_0] - wOp2,0) << ";" << endl; //pad to the right with 0 (xst)
					}else{
						vhdl << tab << declare("sX", l18[k1_1]) << " <= " << op1 << " & " << zg(l18[k1_1] -wOp1,0) << ";" << endl; //pad to the right with 0 (xst)
						vhdl << tab << declare("sY", l25[k2_1]) << " <= " << op2 << " & " << zg(l25[k2_1] -wOp2,0) << ";" << endl;
					}
					int x = 17;
					int y = 24;
				
					REPORT(DEBUG, "number of chunks for first  operand = " << cOp1);
					REPORT(DEBUG, "number of chunks for second operand = " << cOp2);
					
					setCriticalPath( getMaxInputDelays(inputDelays));
					
					/* chunk splitting the first operand into chunks of 17 and 24 bits*/
					for (int k=0; k < cOp1 ; k++)
						vhdl << tab << declare(join("x",k), x+1) << " <= " << (k<cOp1-1?"\"0\" & ":"") << "sX" << range((k<cOp1-1?(k+1)*x:xS)-1,k*x) << ";" << endl;
					for (int k=0; k < cOp2 ; k++)
						vhdl << tab << declare(join("y",k), y+1) << " <= " << (k<cOp2-1?"\"0\" & ":"") << "sY" << range((k<cOp2-1?(k+1)*y:yS)-1,k*y) << ";" << endl;
	
					//MULTIPLICATIONS WITH SOME ACCUMULATIONS
					for (int i=0; i < cOp2; i++){ 
						setCriticalPath ( getMaxInputDelays(inputDelays) );
						for (int j=0; j < cOp1; j++){ 
							if (j==0){ // @ the first the operation is only multiplication, not MAC
								manageCriticalPath(target->DSPMultiplierDelay());
								vhdl << tab << declare(join("px",j,"y",i), x+y+2) << " <= " << join("x",j) << " * " << join("y",i) << ";" << endl;
							}else{
								manageCriticalPath(target->DSPlocalWireDelay() + target->DSPAdderDelay());
								vhdl << tab << declare(join("tpx",j,"y",i),x+y+2) << " <= " << join("x",j) << " * " << join("y",i) << ";" << endl; 
								vhdl << tab << declare(join("px",j,"y",i), x+y+2) << " <= " << join("tpx",j,"y",i) << " + " 
									                                                        << join("px",j-1,"y",i) << range(x+y+1,x) << ";" << endl; 
							}
//							if (!((j==cOp1-1) && (i<cOp2-1))) nextCycle();
						}
						if (i<cOp2-1) setCycle(0); //reset cycle
					}

					//FORM THE INTERMEDIARY PRODUCTS
					for (int i=0; i < cOp2 ; i++){
						vhdl << tab << declare(join("sum",i), x*cOp1 + y + 2) << " <= "; 
						for (int j=cOp1-1;j>=0; j--)
							vhdl << join("px",j,"y",i) << (j<cOp1-1?range(x-1,0):"") << (j==0 ? ";" : " & ");
						vhdl << endl;
					}
				
					if (cOp2 > 1){
						/* more than one operand in the final adder */
						vhdl << tab << declare ("sum0Low", y) << " <= sum0" << range(y-1,0) << ";" << endl;

						manageCriticalPath(target->DSPinterconnectWireDelay());
						REPORT( DEBUG, "The delay at compressor tree input is = " << getCriticalPath());
//						IntNAdder* add =  new IntNAdder(target, (xS + y + 1) + (cOp2-1)*y - y, cOp2);
						IntCompressorTree* add =  new IntCompressorTree(target, (xS + y + 1) + (cOp2-1)*y - y, cOp2, inDelayMap("X0",getCriticalPath()));
						oplist.push_back(add);
	
						/* prepare operands */
						for (int i=0; i < cOp2; i++){
							if (i==0){ 
								vhdl << tab << declare (join("addOp",i), (xS + y + 1) + (cOp2-1)*y - y) << " <= "
									        << rangeAssign( (cOp2-1-i)*y-1, 0, join("sum",i)+of(xS+y+1-1))  << " & " 
									        << join("sum",i) << range(xS+y+1-1,y) << ";" <<endl;
							}else if ((i==1) && (i!=cOp1-1))  { 
								vhdl << tab << declare (join("addOp",i), (xS + y + 1) + (cOp2-1)*y - y) << " <= " 
									        << rangeAssign( (cOp2-1-i)*y-1, 0, join("sum",i)+of(xS+y+1-1))  << ((cOp2-1-i)*x - 1>=0?" & ":"") 
									        << join("sum",i) << ";" <<endl;
							}else if ((i > 1) && ( i!= cOp2-1)){ 
								vhdl << tab << declare (join("addOp",i), (xS + y + 1) + (cOp2-1)*y - y) << " <= " 
									        << rangeAssign( (cOp2-1-i)*y-1, 0, join("sum",i)+of(xS+y+1-1))  << ((cOp2-1-i)*x - 1>=0?" & ":"") 
									        << join("sum",i) << " & "
									        << zg( (i-1)*y, 0) << ";" <<endl;
							}else if (i == cOp2-1){
								vhdl << tab << declare (join("addOp",i), (xS + y + 1) + (cOp2-1)*y - y) << " <= " 
									        << join("sum",i)  << " & "
									        << zg( (i-1)*y, 0) << ";" <<endl;
							}
						}//TODO compact ifs

						for (int i=0; i< cOp2; i++)
							inPortMap (add, join("X",i) , join("addOp",i));
	
//						inPortMapCst(add, "Cin", "'0'");
						outPortMap(add, "R", "addRes");
						vhdl << instance(add, "adder");

						syncCycleFromSignal("addRes");
						outDelayMap["R"] = (add->getOutDelayMap())["R"];

						if ( ( xS+ yS ) - (wInX + wInY) < y ){
							vhdl << tab << "R <= addRes & sum0Low "<<range(y-1, xS + yS - (wInX + wInY) )<< ";" << endl;
						}else{	
							vhdl << tab << "R <= addRes"<<range((xS + y + 1) + (cOp2-1)*y - y -1 , (xS + y + 1) + (cOp2-1)*y - y - (wInX + wInY))<< ";" << endl;
						}
					}else{
						vhdl << tab << "R <= sum0"<<range(xS+yS-1, xS+yS - (wInX + wInY)) << ";" << endl;
						outDelayMap["R"] = getCriticalPath();
					}
				}else{
					setCriticalPath( getMaxInputDelays(inputDelays));
					manageCriticalPath( target->DSPMultiplierDelay());
					vhdl << tab << "R <= X * Y ;" <<endl;
				}
			}else{
				REPORT(0, "WARNINNG: Only implemented for Xilinx Targets for now");
				setCriticalPath( getMaxInputDelays(inputDelays));
				manageCriticalPath( target->DSPMultiplierDelay());
				vhdl << tab << "R <= X * Y;"<<endl;
			}
		}
		else
			vhdl << tab << "R <= X * Y;" <<endl;
		
		if (target_freq > maxDSPFrequency)
			target->setFrequency( target_freq );
	}

	SignedIntMultiplier::~SignedIntMultiplier() {
	}
	
	void SignedIntMultiplier::outputVHDL(std::ostream& o, std::string name) {
		licence(o);
		o << "library ieee; " << endl;
		o << "use ieee.std_logic_1164.all;" << endl;
		o << "use ieee.std_logic_arith.all;" << endl;
		o << "use ieee.std_logic_signed.all;" << endl;
		o << "library work;" << endl;
		outputVHDLEntity(o);
		newArchitecture(o,name);
		o << buildVHDLComponentDeclarations();	
		o << buildVHDLSignalDeclarations();
		beginArchitecture(o);		
		o<<buildVHDLRegisters();
		o << vhdl.str();
		endArchitecture(o);
	}
	

	void SignedIntMultiplier::emulate(TestCase* tc)
	{
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");
		
		mpz_class big1 = (mpz_class(1) << (wInX_));
		mpz_class big1P = (mpz_class(1) << (wInX_-1));
		mpz_class big2 = (mpz_class(1) << (wInY_));
		mpz_class big2P = (mpz_class(1) << (wInY_-1));

		if ( svX >= big1P)
			svX = svX-big1;

		if ( svY >= big2P)
			svY = svY -big2;
			
		mpz_class svR = svX * svY;
		if ( svR < 0){
			mpz_class tmpSUB = (mpz_class(1) << (wOut_));
			svR = tmpSUB + svR; 
		}

		tc->addExpectedOutput("R", svR);
	}
}
