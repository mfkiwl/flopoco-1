/*
 * A generic class for tables of values
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
#include <cstdlib>
#include "utils.hpp"
#include "Table.hpp"

using namespace std;




int Table::double2input(double x){
	cerr << "Error, double2input is being used and has not been overriden";
	return 1;
}

double Table::input2double(int x) {
	cerr << "Error, input2double is being used and has not been overriden";
	return 1;
}

mpz_class Table::double2output(double x){
	cerr << "Error, double2output is being used and has not been overriden";
	return 0;
}

double Table::output2double(mpz_class x) {
	cerr << "Error, output2double is being used and has not been overriden";
	return 1;
}


Table::Table(Target* target, int _wIn, int _wOut, int _minIn, int _maxIn) : 
	Operator(target),
	wIn(_wIn), wOut(_wOut), minIn(_minIn), maxIn(_maxIn)
	{

	// Set up the IO signals
	addInput ("X"  , wIn);
	addOutput ("Y"  , wOut);
	if(maxIn==-1) maxIn=(1<<wIn)-1;
	if(minIn<0) {
		cerr<<"ERROR in Table::Table, minIn<0\n";
		exit(EXIT_FAILURE);
	}
	if(maxIn>=(1<<wIn)) {
		cerr<<"ERROR in Table::Table, maxIn too large\n";
		exit(EXIT_FAILURE);
	}
	if((minIn==0) && (maxIn==(1<<wIn)-1)) 
		full=true;
	else
		full=false;
	if (wIn > 10)
		cerr << "WARNING : FloPoCo is building a table with " << wIn << " input bits, it will be large." << endl;
	}



void Table::outputVHDL(ostream& o, string name)
{

	licence(o,"Florent de Dinechin (2007)");
	Operator::stdLibs(o);
	outputVHDLEntity(o);
	newArchitecture(o,name);
	outputVHDLSignalDeclarations(o);
	beginArchitecture(o);

	int i,x;
	mpz_class y;
	o	<< "  with x select  y <= " << endl;


	for (x = minIn; x <= maxIn; x++) {
		y=this->function(x);
		//    cout << x <<"  "<< y << endl;
		o 	<< tab << "\"";
			printBinPosNumGMP(o, y, wOut);
		o << "\" when \"";
		printBinNum(o, x, wIn);
		o << "\"," << endl;
	}
	o << tab << "\"";
	for (i = 0; i < wOut; i++) o << "-";
	o <<  "\" when others;" << endl;

	endArchitecture(o);
}


int Table::size_in_LUTs() {
	return wOut*int(intpow2(wIn-target_->lutInputs()));
}
