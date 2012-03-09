// works only with sollya
#ifdef HAVE_SOLLYA

#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
  There will be used in the emulate function to manipulate arbitraly large
  entries */
#include "gmp.h"
#include "mpfr.h"
#include <assert.h>
// for debug
#include <signal.h>

// include the header of the Operator
#include "FixSinCos.hpp"

#include "FloPoCo.hpp"

using namespace std;
using namespace flopoco;


// personalized parameter
//string FixSinCos::operatorInfo = "FixSinCos w <options>";



FixSinCos::FixSinCos(Target * target, int w_):Operator(target), w(w_)
{
/* constructor of the FixSinCos
  Target is the targeted FPGA : Stratix, Virtex ... (see Target.hpp for more informations)
  w and param1 are some parameters declared by this Operator developpers, 
  any number can be declared, you will have to modify 
      -> this function,  
      -> the prototype of this function (available in FixSinCos.hpp)
      -> the lines in main.cpp where the command line arguments are parsed in order to generate this FixSinCos
*/
	/* In this constructor we are going to generate an operator that takes as input three bit vectors X,Y,Z of lenght w, treats them as unsigned integers, sums them and then output the last param1 bit of the sum adding the first bit of the sum (most significant) in front of this output, all the vhdl code needed by this operator has to be generated in this function */

	// definition of the name of the operator
	ostringstream name;
	name << "FixSinCos_" << w	//<< "_" << param1
	    ;
	setName(name.str());

	setCopyrightString("Guillaume Sergent 2012");

	/* SET UP THE IO SIGNALS
	   Each IO signal is declared by addInput(name,n) or addOutput(name,n) 
	   where name is a string that stands for the name of the variable and 
	   n is an integer (int)   that stands for the length of the corresponding 
	   input/output */

	// declaring inputs
	addInput("X", w);
	//addFullComment(" addFullComment for a large comment ");
	//addComment("addComment for small left-aligned comment");

	// declaring outputs
	addOutput("S", w+1);
	addOutput("C", w+1);


	/* Some peace of informations can be delivered to the flopoco user if  the -verbose option is set
	   [eg: flopoco -verbose=0 FixSinCos 10 5 ]
	   , by using the REPORT function.
	   There is three level of details
	   -> INFO for basic information ( -verbose=1 )
	   -> DETAILED for complete information, includes the INFO level ( -verbose=2 )
	   -> DEBUG for complete and debug information, to be used for getting information 
	   during the debug step of operator development, includes the INFO and DETAILED levels ( -verbose=3 )
	 */
	// basic message
	//REPORT(INFO,"Declaration of FixSinCos \n");

	// more detailed message
	//ostringstream detailedMsg;
	//detailedMsg  << "this operator has received two parameters " << w << " and " << param1;
	//REPORT(DETAILED,detailedMsg.str());

	// debug message for developper
	//REPORT(DEBUG,"debug of FixSinCos");



	/* vhdl is the stream which receives all the vhdl code, some special functions are
	   available to smooth variable declaration and use ... 
	   -> when first using a variable (Eg: T), declare("T",64) will create a vhdl
	   definition of the variable : signal T and includes it it the header of the architecture definition of the operator

	   Each code transmited to vhdl will be parsed and the variables previously declared in a previous cycle will be delayed automatically by a pipelined register.
	 */
	//vhdl << declare("T",
	//		w + 1) << " <= ('0' & X) + ('O' & Y);" << endl;

	// the argument is reduced into (0,1/4) because one sin/cos
	// computation in this range can always compute the right sin/cos
	vhdl << declare ("A",1) << " <= X" << of (w-1) << ";" << endl
	     << declare ("B",1) << " <= X" << of (w-2) << ";" << endl
	     << declare ("Y",w-2) << " <= X " << range (w-3,0) << ";"
	     << endl;
	// now X -> A*.5 + B*.25 + Y where A,B \in {0,1} and Y \in {0,.25}

	// Y_prime = .25 - y
	// the VHDL is probably invalid
	// perhaps do a logic ~ at a cost of 1 ulp ?
	vhdl << declare ("Y_prime",w-2) << " <= "
	     << "2**" << (w-2) << " - Y;" << endl;

	// Y_in = / Y_prime if B=1
	//        \ Y if B=0
	vhdl << declare ("Y_in",w-2) << " <= Y_prime when B='1' else Y;"
	     << endl;

	// Exch = A ^ B
	vhdl << declare ("Exch",1) << " <= A xor B;" << endl;

	// now we do manual polynomial computation to calculate sin (pi*y)
	// and cos (pi*y) using Taylor polynomials: with y'=pi*y
	// sin (4*pi*y) = y' - y'³/6
	// cos (4*pi*y) = 1 - y'²/2
	// this works if y' (or y) is small enough
	// to accomplish this we decompose x (==Y_in in the vhdl) to x = a + y
	// where a \in [0,1/4[ and {sin,cos} (pi*a) is tabulated
	// and y \in [0,1b-n[ is a small enough argument
	// then we use the addition formulae (where Sin(x)=sin(pi*x)):
	// Sin (a+y) = Sin a Cos y + Cos a Sin y
	//           = Sin a - Sin a (1 - Cos y) + Cos a Sin y
	// Cos (a+y) = Cos a Cos y - Sin a Sin y
	//           = Cos a - Cos a (1 - Cos y) - Sin a Sin y
	const int g=3; //guard bits
	int wIn = w-2+g;
	// wA is the precision of A, beginning from the ,..[] bit
	// yes I know it's moronic to int->float->int
	// pi⁴/24=4.0587121 so (pi*y)⁴/24 is < (1+eps) ulp /this is not the thing to do actually/
	// iff 4 * (wA+2) + 2 >= wIn+2 (2 as the log_2 of pi⁴/24)
	// the minimal a is therefore ceil((wIn)/4) - 2
	int wA = (int) ceil ((float) (wIn)/4.) - 2;
	int wY = wIn-wA, wZ = wY+2;
	// vhdl:split (Y_in -> A_tbl & Y_red)
	vhdl << declare ("A_tbl",wA) << " <= Y_in" << range (w-3,w-2-wA) << ";" << endl
	     << declare ("Y_red",wY) << " <= Y_in" << range (w-3-wA,0);
	for (int i = 0; i < g; i ++) {
		vhdl << " & '0'";
	}
	vhdl << ';' << endl;
	// vhdl:lut (A_tbl -> A_cos_pi_tbl, A_sin_pi_tbl)
	// the results have precision w+g
	// now, evaluate Sin Y_red and 1 - Cos Y_red
	// vhdl:cmul[pi] (Y_red -> Z)
	FixRealKCM *pi_mult;
	// the 2 extra bits to Z are added by the KCM multiplier
	pi_mult = new FixRealKCM (target, 0, wY-1, false, 0, "pi"); 
	oplist.push_back (pi_mult);
	inPortMap (pi_mult, "X", "Y_red");
	outPortMap (pi_mult, "R", "Z");
	vhdl << instance (pi_mult, "pi_mult");
	// vhdl:sqr (Z -> Z_2)
	// we have no truncated squarer as of now
	IntSquarer *sqr_z;
	sqr_z = new IntSquarer (target, wY);
	oplist.push_back (sqr_z);
	inPortMap (sqr_z, "X", "Z");
	outPortMap (sqr_z, "R", "Z_2_ext");
	vhdl << instance (sqr_z, "sqr_z");
	// so now we truncate unnecessarily calculated bits of Z_2_ext
	int wZ_2 = 2*wZ - (w+g);
	vhdl << declare ("Z_2",wZ_2) << " <= Z_2_ext"
	     << range (wZ_2-1,0) << ";" << endl;
	// vhdl:mul (Z, Z_2 -> Z_3)
	IntTruncMultiplier *z_3;
	int wZ_3 = 3*wZ - 2*(w+g);
	z_3 = new IntTruncMultiplier (target, wZ, wZ_2, wZ_3,
	                              1.f, 0, 0); //last params wtf?
	oplist.push_back (z_3);
	inPortMap (z_3, "X", "Z");
	inPortMap (z_3, "Y", "Z_2");
	outPortMap (z_3, "R", "Z_3");
	vhdl << instance (z_3, "z_3");
	// vhdl:slr (Z_2 -> Z_cos_red)
	int wZ_cos_red = wZ_2 - 1;
	vhdl << declare ("Z_cos_red", wZ_2-1)
	     << " <= Z_2" << range (wZ_2-1,1) << ";" << endl;
	// vhdl:cmul[1/6] (Z_3 -> Z_3_6)
	vhdl << declare ("Z_3_2", wZ_3-1)
	     << " <= Z_3" << range (wZ_3-1,1) << ";" << endl;
	IntConstDiv *cdiv_3;
	cdiv_3 = new IntConstDiv (target, wZ_3-1, 3, -1);
	oplist.push_back (cdiv_3);
	inPortMap (cdiv_3, "X", "Z_3_2");
	outPortMap (cdiv_3, "Q", "Z_3_6");
	vhdl << instance (cdiv_3, "cdiv_3");
	// vhdl:sub (Z, Z_3_6 -> Z_sin)
	vhdl << declare ("Z_sin", wZ)
	     << " <= Z - Z_3_6;" << endl;
	// and now, evaluate Sin Y_in and Cos Y_in
	// Cos Y_in:
	// // vhdl:id (A_cos_pi_tbl -> C_out_1)
	// vhdl:mul (Z_cos_red, A_cos_pi_tbl -> C_out_2)
	IntTruncMultiplier *c_out_2;
	c_out_2 = new IntTruncMultiplier (target, wZ_cos_red, w+g, wZ_cos_red,
	                                  1.f, 0, 0);
	oplist.push_back (c_out_2);
	inPortMap (c_out_2, "X", "Z_cos_red");
	inPortMap (c_out_2, "Y", "A_cos_pi_tbl");
	outPortMap (c_out_2, "R", "C_out_2");
	vhdl << instance (c_out_2, "c_out_2");
	// vhdl:mul (Z_sin, A_sin_pi_tbl -> C_out_3)
	IntTruncMultiplier *c_out_3;
	c_out_3 = new IntTruncMultiplier (target, wZ, w+g, wZ,
	                                  1.f, 0, 0);
	oplist.push_back (c_out_3);
	inPortMap (c_out_3, "X", "Z_sin");
	inPortMap (c_out_3, "Y", "A_sin_pi_tbl");
	outPortMap (c_out_3, "R", "C_out_3");
	vhdl << instance (c_out_3, "c_out_3");
	// vhdl:add (C_out_2, C_out_3 -> C_out_4)
	vhdl << declare ("C_out_4", wZ)
	     << " <= C_out_2 + C_out_3;" << endl;
	// vhdl:sub (A_cos_pi_tbl, C_out_4 -> C_out)
	// C_out has the entire precision; _g because it still has guards
	vhdl << declare ("C_out_g", w+g)
	     << " <= A_cos_pi_tbl - C_out_4;" << endl;
	// Sin Y_in:
	// // vhdl:id (A_sin_pi_tbl -> S_out_1)
	// vhdl:mul (Z_cos_red, A_sin_pi_tbl -> S_out_2)
	IntTruncMultiplier *s_out_2;
	s_out_2 = new IntTruncMultiplier (target, wZ_cos_red, w+g, wZ_cos_red,
	                                  1.f, 0, 0);
	oplist.push_back (s_out_2);
	inPortMap (s_out_2, "X", "Z_cos_red");
	inPortMap (s_out_2, "Y", "A_sin_pi_tbl");
	outPortMap (s_out_2, "R", "S_out_2");
	vhdl << instance (s_out_2, "s_out_2");
	// vhdl:mul (Z_sin, A_cos_pi_tbl -> S_out_3)
	IntTruncMultiplier *s_out_3;
	s_out_3 = new IntTruncMultiplier (target, wZ, w+g, wZ,
	                                  1.f, 0, 0);
	oplist.push_back (s_out_3);
	inPortMap (s_out_3, "X", "Z_sin");
	inPortMap (s_out_3, "Y", "A_cos_pi_tbl");
	outPortMap (s_out_3, "R", "S_out_3");
	vhdl << instance (s_out_3, "s_out_3");
	// vhdl:add (A_sin_pi_tbl, S_out_3 -> S_out_4)
	vhdl << declare ("S_out_4", w+g) //w+g necessary because of sin(pi*a)
	     << " <= A_sin_pi_tbl + S_out_3" << endl;
	// vhdl:sub (S_out_4, S_out_2 -> S_out)
	vhdl << declare ("S_out_g", w+g)
	     << " <= S_out_4 - S_out_2" << endl;

	// now remove the guard bits
	vhdl << declare ("C_out", w)
	     << " <= C_out_g" << range (w+g-1, g) << endl
	     << declare ("S_out", w)
	     << " <= S_out_g" << range (w+g-1, g) << endl;

	// now just add the signs to the signals
	/* the output format is consequently
	 * struct output_format {
	 *	unsigned int sgn: 1; // sign is 1 for neg, as in IEEE754
	 *	unsigned int mantissa: w;
	 * };
	 */
	vhdl << "S <= '0' & (C_out when Exch else S_out);" << endl;
	vhdl << "C <= A & (S_out when Exch else C_out);" << endl;


	/* declaring a new cycle, each variable used after this line will be delayed 
	   with respect to his state in the precedent cycle
	 */
	//nextCycle();
	//vhdl << declare("R",
	//		w + 2) << " <=  ('0' & T) + (\"00\" & Z);" << endl;

	/* the use(variable) is a deprecated function, that can still be encoutered 
	   in old Flopoco's operators, simply use vhdl << "S" (flopoco will generate the correct delayed value as soon as a previous declare("S") exists */
	// we first put the most significant bit of the result into R
	//vhdl << "S <= (R" << of(w + 1) << " & ";
	// and then we place the last param1 bits
	//vhdl << "R" << range(param1 - 1, 0) << ");" << endl;
};


// doesn't work yet
void FixSinCos::emulate(TestCase * tc)
{
	mpz_class sx = tc->getInputValue ("X");
	mpfr_t x, sind, cosd, sinu, cosu, pixd, pixu;
	mpz_t sind_z, cosd_z, sinu_z, cosu_z;
	mpfr_init2 (x, 1+w);
	mpz_init2 (sind_z, 1+w);
	mpz_init2 (cosd_z, 1+w);
	mpz_init2 (sinu_z, 1+w);
	mpz_init2 (cosu_z, 1+w);
	mpfr_init (sind); // 1 extra of precision to have <½ulp(x) error
	mpfr_init (cosd);
	mpfr_init (sinu); // 1 extra of precision to have <½ulp(x) error
	mpfr_init (cosu);
	// 1 extra bit (plus 11. beginning) will guarantee <½ulp(x) error on pi*x
	mpfr_init (pixd);
	mpfr_init (pixu);

	mpfr_set_z (x, sx.get_mpz_t(), GMP_RNDD); // this rounding is exact
	mpfr_div_2si (x, x, w, GMP_RNDD); // this rounding is acually exact
	int i=0, ep; // ep: extra precision
	do {
		ep = 1 << i;
		mpfr_set_prec (sind, 1+w+ep); // 1 extra of precision to have <½ulp(x) error
		mpfr_set_prec (cosd, 1+w+ep);
		mpfr_set_prec (sinu, 1+w+ep); // 1 extra of precision to have <½ulp(x) error
		mpfr_set_prec (cosu, 1+w+ep);
		// 1 extra bit (plus 11. beginning) will guarantee <½ulp(x) error on pi*x
		mpfr_set_prec (pixd, 2+w+ep);
		mpfr_set_prec (pixu, 2+w+ep);
		mpfr_const_pi (pixd, GMP_RNDD);
		mpfr_const_pi (pixu, GMP_RNDU);
		mpfr_mul (pixd, pixd, x, GMP_RNDD);
		mpfr_mul (pixu, pixu, x, GMP_RNDU);
		if (mpfr_cmp_ui_2exp (x, 1UL, -1) > 0) { // if (x < 0.5f)
			// then sin is increasing near x
			mpfr_sin (sind, pixd, GMP_RNDD);
			mpfr_sin (sinu, pixu, GMP_RNDU);
		} else {
			// then sin is decreasing near x
			mpfr_sin (sind, pixu, GMP_RNDD); // use the upper x for the lower sin
			mpfr_sin (sinu, pixd, GMP_RNDU);
		}
		mpfr_cos (cosd, pixu, GMP_RNDD); // cos decreases from 0 to pi
		mpfr_cos (cosu, pixd, GMP_RNDU);
		// if the cosine is <0 we must neg it then add 1
		if (mpfr_cmp_ui (cosd, 0) < 0) {
			mpfr_setsign (cosd, cosd, 0, GMP_RNDD); // exact rnd
			mpfr_add_ui (cosd, cosd, 1, GMP_RNDD); // exact rnd
		}
		// same as before
		if (mpfr_cmp_ui (cosu, 0) < 0) {
			mpfr_setsign (cosu, cosu, 0, GMP_RNDU);
			mpfr_add_ui (cosu, cosu, 1, GMP_RNDU);
		}
		mpfr_mul_2si (sind, sind, w, GMP_RNDD); // exact rnd here
		mpfr_mul_2si (cosd, cosd, w, GMP_RNDD);
		mpfr_mul_2si (sinu, sinu, w, GMP_RNDU);
		mpfr_mul_2si (cosu, cosu, w, GMP_RNDU);
		mpfr_get_z (sind_z, sind, GMP_RNDD); // there can be a real rounding here
		mpfr_get_z (cosd_z, cosd, GMP_RNDD);
		mpfr_get_z (sinu_z, sinu, GMP_RNDU); // there can be a real rounding here
		mpfr_get_z (cosu_z, cosu, GMP_RNDU);
		// now we test if the upper results are the lower ones + 1
		// as we want them to
		mpz_sub_ui (sinu_z, sinu_z, 1UL);
		mpz_sub_ui (cosu_z, cosu_z, 1UL);
		if (mpz_cmp (sind_z, sinu_z) == 0 &&
		    mpz_cmp (cosd_z, cosu_z) == 0) {
			// the rounding are really what we want
			break;
		} else {
			i++;
		}
	} while (i<16); // for sanity
	mpz_class sind_zc (sind_z), cosd_zc (cosd_z);
	// and since FunctionEvaluator does only faithful rounding
	// we add also as expected results the upper roundings
	tc->addExpectedOutput ("S", sind_zc);
	tc->addExpectedOutput ("C", cosd_zc);
	tc->addExpectedOutput ("S", sind_zc+1);
	tc->addExpectedOutput ("C", cosd_zc+1);
	// not complete yet
	mpfr_clears (x, sind, cosd, NULL);
}


void FixSinCos::buildStandardTestCases(TestCaseList * tcl)
{
}

void FixSinCos::buildRandomTestCases(TestCaseList * tcl, int n)
{
}

TestCase *FixSinCos::buildRandomTestCases(int i)
{
	TestCase *tc = new TestCase(this);
	return tc;
}

#endif // SOLLYA
