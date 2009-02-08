#ifndef KARATSUBA_HPP
#define KARATSUBA_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"

/** 
 * The Integer Multiplier class. Receives at input two numbers of 
 * wInX and wInY widths and outputs a result having the width wOut=wInX+wInY 
 **/
class Karatsuba : public Operator
{
public:
	/** 
	 * The constructor of the Karatsuba class
	 * @param target argument of type Target containing the data for which this operator will be optimized
	 * @param wInX integer argument representing the width in bits of the input X 
	 * @param wInY integer argument representing the width in bits of the input Y
	 **/
	Karatsuba(Target* target, int wInX, int wInY);
	
	/**
	 * Karatsuba destructor
	 */
	~Karatsuba();

	/**
	 * Method which generates the code for the combinational version of of the Karatsuba architecture
	 * @param[in,out] o           the stream where the code generated by the method is returned to
	 * @param[in]     L           the left index of the lName and rName arrays which will be considered as the MSB of the inputs
	 * @param[in]     R           the right index of the lName and rName arrays which will be considered as the LSB of the inputs. 
	 *                            For example, if lName="X" and rName="Y", this method will compute the product X(L downto R)*Y(L downto R)
	 * @param[in]     lName       the name of the left term of the product
	 * @param[in]     rName       the name of the right term of the product
	 * @param[in]     resultName  the name of the product. Adding a signal with this name and the correct size is a responasbility of the calling function.
	 * @param[in]     depth       the recursivity depth
	 * @param[in]     branch      the branch we are currently on inside the recursivity. 
	 *                            The main branch is named mb, the central branch cb, left branch lb and right branch rb
	 **/
	void BuildCombinationalKaratsuba(std::ostream& o, int L, int R, string lName, string rName, string resultName, int depth, string branch);

	/**
	 * Method belonging to the Operator class overloaded by the Karatsuba class
	 * @param[in,out] o     the stream where the current architecture will be outputed to
	 * @param[in]     name  the name of the entity corresponding to the architecture generated in this method
	 **/
	void outputVHDL(std::ostream& o, std::string name);
	
	/**
	 * Gets the correct value associated to one or more inputs.
	 * @param a the array which contains both already filled inputs and
	 *          to be filled outputs in the order specified in getTestIOMap.
	 */
	void fillTestCase(mpz_class a[]);

	/** 
	 * Sets the default name of this operator
	 */
	void setOperatorName(); 

protected:	
	
	int wInX_; /**< the width (in bits) of the input X  */
	int wInY_; /**< the width (in bits) of the input Y  */
	int wOut_; /**< the width (in bits) of the output R  */

private:
	int multiplierXWidth_;
	
};
#endif
