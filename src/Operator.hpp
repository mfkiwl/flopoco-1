#ifndef OPERATOR_HPP
#define OPERATOR_HPP
#include <vector>
#include <map>
#include <gmpxx.h>
#include "Target.hpp"
#include "Signal.hpp"
#include "TestCase.hpp"

using namespace std;

// variables set by the command-line interface in main.cpp


namespace flopoco {

	// Reporting levels
#define INFO 1
#define DETAILED 2
#define DEBUG 3
#define REPORT(level, stream) { if ((level)<=(verbose)) cerr << "> " << srcFileName << ": " << stream << endl;} 



extern  int verbose;
const std::string tab = "   ";

 /*****************************************************************************/
 /*                        SORRY FOR THE INCONVENIENCE                        */
 /*****************************************************************************/
/*
 This version of the Operator class is in a transitional state.
 It contains methods for both the new pipeline framework (pipeline made easy)
 and the old one (flagged as DEPRECATED). 
 The developer manual describes only the new approach, which is so much better
  (much shorter code and very easy pipeline setup).

 Beware, when cut'n-pasting from existing code, that an operator
 should not mix both approaches. Please cut'n past from code using
 declare() and use(), e.g. FPAdder.

*/





/**
 * This is a top-level class representing an Operator.
 * This class is inherited by all classes which will output a VHDL entity.
 */
class Operator
{
public:
	/** Default constructor.
	 * Creates a default operator instance. No parameters are passed to this constructor.
	 */
	Operator(){
		numberOfInputs_             = 0;
		numberOfOutputs_            = 0;
		hasRegistersWithoutReset_   = false;
		hasRegistersWithAsyncReset_ = false;
		hasRegistersWithSyncReset_  = false;
		pipelineDepth_              = 0;
		currentCycle_                      = 0;
	}
	
	/** Operator Constructor.
	 * Creates an operator instance with an instantiated target for deployment.
	 * @param target_ The deployment target of the operator.
	 */
	Operator(Target* target)  {
		target_                     = target;
		numberOfInputs_             = 0;
		numberOfOutputs_            = 0;
		hasRegistersWithoutReset_   = false;
		hasRegistersWithAsyncReset_ = false;
		hasRegistersWithSyncReset_  = false;
		pipelineDepth_              = 0;
		currentCycle_                      = 0;
		if (target_->isPipelined())
			setSequential();
		else
			setCombinatorial();	
	}

	/** Operator Destructor.
	 */	
	virtual ~Operator() {}


 /*****************************************************************************/
 /*         Paperwork-related methods (for defining an operator entity)       */
 /*****************************************************************************/

	/** Adds an input signal to the operator.
	 * Adds a signal of type Signal::in to the the I/O signal list.
	 * @param name  the name of the signal
	 * @param width the number of bits of the signal.
	 * @param isBus describes if this signal is a bus, that is, an instance of std_logic_vector
	 */
	void addInput  (const std::string name, const int width=1, const bool isBus=false);
	

	/** Adds an output signal to the operator.
	 * Adds a signal of type Signal::out to the the I/O signal list.
	 * @param name  the name of the signal
	 * @param width the number of bits of the signal.
	 * @param numberOfPossibleOutputValues (optional, defaults to 1) set to 2 for a faithfully rounded operator for instance
	 * @param isBus describes if this signal is a bus, that is, an instance of std_logic_vector
	 */	
	void addOutput(const std::string name, const int width=1, const int numberOfPossibleOutputValues=1, const bool isBus=false);
	

	/** Adds a floating point input signal to the operator.
	 * Adds a signal of type Signal::in to the the I/O signal list, 
	 * having the FP flag set on true. The total width of this signal will
	 * be wE + wF + 3. (2 bits for exception, 1 for sign)
	 * @param name the name of the signal
	 * @param wE   the width of the exponent
	 * @param wF   the withh of the fraction
	 */
	void addFPInput(const std::string name, const int wE, const int wF);


	/** Adds a floating point output signal to the operator.
	 * Adds a signal of type Signal::out to the the I/O signal list, 
	 * having the FP flag set on true. The total width of this signal will
	 * be wE + wF + 3. (2 bits for exception, 1 for sign)
	 * @param name the name of the signal
	 * @param wE   the width of the exponent
	 * @param wF   the withh of the fraction
	 * @param numberOfPossibleOutputValues (optional, defaults to 1) set to 2 for a faithfully rounded operator for instance
	 */	
	void addFPOutput(const std::string name, const int wE, const int wF, const int numberOfPossibleOutputValues=1);

	

	/** sets the copyright string, should be authors + year
	 * @param authorsYears the names of the authors and the years of their contributions
	 */
	void setCopyrightString(std::string authorsYears);


	/** Sets Operator name to givenName.
	 * Sets the name of the operator to operatorName.
	 * @param operatorName new name of the operator
	*/
	void setName(std::string operatorName = "UnknownOperator");
	
	/** This method should be used by an operator to change the default name of a sub-component. The default name becomes the commented name.
	 * @param operatorName new name of the operator
	*/
	void changeName(std::string operatorName);
	
	/** Sets Operator name to prefix_(uniqueName_)_postfix
	 * @param prefix the prefix string which will be placed in front of the operator name
	 *               formed with the operator internal parameters
	 * @param postfix the postfix string which will be placed at the end of the operator name
	 *                formed with the operator internal parameters
	*/
	void setName(std::string prefix, std::string postfix);
		
	/** Return the operator name. 
	 * Returns a string value representing the name of the operator. 
	 * @return operator name
	 */
	string getName() const;




 /*****************************************************************************/
 /*        VHDL-related methods (for defining an operator architecture)       */
 /*****************************************************************************/



	/* Functions related to pipeline management */

	/** Define the current cycle 
	 * @param the new value of the current cycle */
	void setCycle(int cycle, bool report=true) ;

	/** Return the current cycle 
	 * @return the current cycle */
	int getCurrentCycle(); 

	/** Define the current cycle 
	 * @param the new value of the current cycle */
	void nextCycle(bool report=true) ;

	/** get the critical path of the current cycle so far */
	double getCriticalPath() ;

	/** Set or reset the critical path of the current cycle  */
	void setCriticalPath(double delay) ;

	/** Adds to the critical path of the current stage, and insert a pipeline stage if needed
	 * @param the delay to add to the critical path of current pipeline stage */
	void addToCriticalPath(double delay) ;

	/** Adds to the critical path of the current stage, and insert a pipeline stage if needed
	 * @param the delay to add to the critical path of current pipeline stage */
	void nextCycleCond(double delay) ;

	/** get the critical path delay associated to a given output of the operator
	 * @param the name of the output */
	double getOutputDelay(string s); 

	/** Set the current cycle to that of a signal. It may increase or decrease current cycle. 
	 * @param name is the signal name. It must have been defined before 
	 * @param report is a boolean, if true it will report the cycle 
	 */
	void setCycleFromSignal(string name, bool report=false) ;

	/** advance the current cycle to that of a signal. It may only increase current cycle. To synchronize
		 two or more signals, first call setCycleFromSignal() on the
		 first, then syncCycleFromSignal() on the remaining ones. It
		 will synchronize to the latest of these signals.  
		 * @param name is the signal name. It must have been defined before 
		 * @param report is a boolean, if true it will report the cycle 
		 */
	void syncCycleFromSignal(string name, bool report=true) ;




	/** Declares a signal implicitely by having it appearing on the Left Hand Side of a VHDL assignment
	 * @param name is the name of the signal
	 * @param width is the width of the signal (optional, default 1)
	 * @param isbus: a signal of width 1 is declared as std_logic when false, as std_logic_vector when true (optional, default false)
	 * @param regType: the registring type of this signal. See also the Signal Class for more info
	 * @return name
	 */
	string declare(string name, const int width=1, bool isbus=false, Signal::SignalType regType = Signal::wire );
	// TODO: add methods that allow for signals with reset (when rewriting LongAcc for the new framework)


	/** use a signal on the Right 
	 * @param name is the name of the signal
	 * @return name
	 */
	string use(string name);




	
	/** Declare an output mapping for an instance of a sub-component
	 * Also declares the local signal implicitely, with width taken from the component 	
	 * @param op is a pointer to the subcomponent
	 * @param componentPortName is the name of the port on the component
	 * @param actualSignalName is the name of the signal in This mapped to this port
	 * @return name
	 */
	void outPortMap(Operator* op, string componentPortName, string actualSignalName);


	/** use a signal as input of a subcomponent
	 * @param componentPortName is the name of the port on the component
	 * @param actualSignalName is the name of the signal (of this) mapped to this port
	 */
	void inPortMap(Operator* op, string componentPortName, string actualSignalName);

	/** use a constant signal as input of a subcomponent. 
	 * @param componentPortName is the name of the port on the component
	 * @param actualSignal is the constant signal to be mapped to this port
	 */
	void inPortMapCst(Operator* op, string componentPortName, string actualSignal);

	/** returns the VHDL for an instance of a sub-component. 
	 * @param op represents the operator to be port mapped 
	 * @param instanceName is the name of the instance as a label
	 * @return name
	 */
	string instance(Operator* op, string instanceName);

	/** returns the VHDL for an instance of a sub-component. 
	 * @param op represents the operator to be port mapped 
	 * @param instanceName is the name of the instance as a label
	 * @param clkName the name of the input clock signal
 	 * @param clkName the name of the input rst signal
	 * @return name
	 */
	string instance(Operator* op, string instanceName, string clkName, string rstName);

	/**
	 * A new architecture inline function
	 * @param[in,out] o 	- the stream to which the new architecture line will be added
	 * @param[in]     name	- the name of the entity corresponding to this architecture
	 **/
	inline void newArchitecture(std::ostream& o, std::string name){
		o << "architecture arch of " << name  << " is" << endl;
	}
	
	/**
	 * A begin architecture inline function 
	 * @param[in,out] o 	- the stream to which the begin line will be added
	 **/
	inline void beginArchitecture(std::ostream& o){
		o << "begin" << endl;
	}

	/**
	 * A end architecture inline function 
	 * @param[in,out] o 	- the stream to which the begin line will be added
	 **/
	inline void endArchitecture(std::ostream& o){
		o << "end architecture;" << endl << endl;
	}




 /*****************************************************************************/
 /*        Testing-related methods (for defining an operator testbench)       */
 /*****************************************************************************/

	/**
	 * Gets the correct value associated to one or more inputs.
	 * @param tc the test case, filled with the input values, to be filled with the output values.
	 * @see FPAdder for an example implementation
	 */
	virtual void emulate(TestCase * tc);
		
	/**
	 * Append standard test cases to a test case list. Standard test
	 * cases are operator-dependent and should include any specific
	 * corner cases you may think of. Never mind removing a standard test case because you think it is no longer useful!
	 * @param tcl a TestCaseList
	 */
	virtual void buildStandardTestCases(TestCaseList* tcl);


	/**
	 * Append random test cases to a test case list. There is a default
	 * implementation using a uniform random generator, but most
	 * operators are not exercised efficiently using such a
	 * generator. For instance, in FPAdder, the random number generator
	 * should be biased to favor exponents which are relatively close
	 * so that an effective addition takes place.
	 * @param tcl a TestCaseList
	 * @param n the number of random test cases to add
	 */
	virtual void buildRandomTestCases(TestCaseList* tcl, int n);


	Target* target() {
		return target_;
	}



 /*****************************************************************************/
 /*     From this point, we have methods that are not needed in normal use    */
 /*****************************************************************************/



	
	/** build all the signal declarations from signals implicitely declared by declare().
	 *  This is the 2.0 equivalent of outputVHDLSignalDeclarations
	 */
	string buildVHDLSignalDeclarations();

	/** build all the component declarations from the list built by instance().
	 */
	string buildVHDLComponentDeclarations();

	/** build all the registers from signals implicitely delayed by declare() 
	 *	 This is the 2.0 equivalent of outputVHDLSignalRegisters
	 */
	string buildVHDLRegisters();


	/** output the VHDL constants. */
	string buildVHDLConstantDeclarations();

	/** output the VHDL constants. */
	string buildVHDLAttributes();





	/** the main function outputs the VHDL for the operator.
		 If you use the modern (post-0.10) framework you no longer need to overload this method,
		 the default will do.
	 * @param o the stream where the entity will outputted
	 * @param name the name of the architecture
	 */
	virtual void outputVHDL(std::ostream& o, std::string name);
	
	/** the main function outputs the VHDL for the operator 
	 * @param o the stream where the entity will outputted
	 */	
	void outputVHDL(std::ostream& o);   // calls the previous with name = uniqueName





	/** True if the operator needs a clock signal; 
	 * It will also get a rst but doesn't need to use it.
	 */	
	bool isSequential();  
	
	/** Set the operator to sequential.
		 You shouldn't need to use this method for standard operators 
		 (Operator::Operator()  calls it according to Target)
	 */	
	void setSequential(); 
	
	/** Set the operator to combinatorial
		 You shouldn't need to use this method for standard operators 
		 (Operator::Operator()  calls it according to Target)
	 */	
	void setCombinatorial();
	





	
	

	/** Returns a pointer to the signal having the name s. Throws an exception if the signal is not yet declared.
	  * @param s then name of the signal we want to return
	  * @return the pointer to the signal having name s 
	  */
	Signal* getSignalByName(string s);






	/** DEPRECATED Outputs component declaration 
	 * @param o the stream where the component is outputed
	 * @param name the name of the VHDL component we want to output to o
	 */
	virtual void outputVHDLComponent(std::ostream& o, std::string name);
	
	/**  DEPRECATED Outputs the VHDL component code of the current operator
	 * @param o the stream where the component is outputed
	 */
	void outputVHDLComponent(std::ostream& o);  
	
	/**  DEPRECATED Function which outputs the processes which declare the registers ( assign name_d <= name )
	 * @param o the stream where the component is outputed
	 */
	void outputVHDLRegisters(std::ostream& o);
	
		

	/** Return the number of input+output signals 
	 * @return the size of the IO list. The total number of input and output signals
	 *         of the architecture.
	 */
	int getIOListSize() const;
	
	/** Returns a pointer to the list containing the IO signals.
	 * @return pointer to ioList 
	 */
	vector<Signal*> * getIOList();
	
	/** Returns a pointer a signal from the ioList.
	 * @param the index of the signal in the list
	 * @return pointer to the i'th signal of ioList 
	 */
	Signal * getIOListSignal(int i);
		
	



	/** DEPRECATED, better use setCopyrightString
		 Output the licence
	 * @param o the stream where the licence is going to be outputted
	 * @param authorsYears the names of the authors and the years of their contributions
	 */
	void licence(std::ostream& o, std::string authorsYears);


	/**  Output the licence, using copyrightString_
	 * @param o the stream where the licence is going to be outputted
	 */
	void licence(std::ostream& o);

	/** DEPRECATED  Output the standard library paperwork 
	 * @param o the stream where the libraries will be written to
	 */
	static void stdLibs(std::ostream& o){
		o<<"library ieee;\nuse ieee.std_logic_1164.all;"<<endl 
		 <<"use ieee.std_logic_arith.all;"<<endl
		 <<"use ieee.std_logic_unsigned.all;"<<endl 
		 <<"library work;"<<endl<<endl;
	};
		
	/** DEPRECATED  Output the VHDL entity of the current operator.
	 * @param o the stream where the entity will be outputted
	 */
	void outputVHDLEntity(std::ostream& o);
	
	/** DEPRECATED  output all the signal declarations 
	 * @param o the stream where the signal deca
	 */
	void outputVHDLSignalDeclarations(std::ostream& o);


	/** Add a VHDL constant. This may make the code easier to read, but more difficult to debug. */
	void addConstant(std::string name, std::string ctype, int cvalue);

	void addConstant(std::string name, std::string ctype, mpz_class cvalue);
	

	/** Add attribute, declaring the attribute name if it is not done already.
	 */ 
	void addAttribute(std::string attributeName,  std::string attributeType,  std::string object, std::string value );

	/**
	 * A new line inline function
	 * @param[in,out] o the stream to which the new line will be added
	 **/
	inline void newLine(std::ostream& o) {	o<<endl; }



	/** Final report function, prints to the terminal.  By default
	 * reports the pipeline depth, but feel free to overload if you have any
	 * thing useful to tell to the end user
	*/
	virtual void outputFinalReport();	
	
	
	/** Gets the pipeline depth of this operator 
	 * @return the pipeline depth of the operator
	*/
	int getPipelineDepth();






	/** DEPRECATED Set the depth of the pipeline
		 You shouldn't need to use this method for standard operators 
		 (Operator::Operator()  calls it according to Target)
	 * @param d the depth of the pipeline for the operator
	 */	
	void setPipelineDepth(int d);
	
	
	/** DEPRECATED Increments the pipeline depth of the current operator 
	*/
	void incrementPipelineDepth();
	


	/** DEPRECATED, use declare() instead Generic function that adds a signal to the signal list.
	 * The functions add*Signal* are implemented as instances of this functions.
	 * @param name  the name of the signal
	 * @param width the width of the signal
	 * @param delay the delay of the signal. The number of register levels that this signal needs to be delayed. If negative, treated as zero (useful for pipeline synchronization)
	 * @param regType the register type (registeredWithoutReset,registeredWithAsyncReset,registeredWithSyncReset, see Signal::SignalType)
	 * @param isBus if true, a signal with a width of 1 is declared as std_logic_vector; if false it is declared as std_logic.
	 */	
	void addSignalGeneric(const string name, const int width, const int delay, Signal::SignalType regType, bool isBus);



	/** DEPRECATED, use declare() instead
		 Adds a signal to the signal list.
	 * Adds a signal of type Signal::wire to the the signal list.
	 * @param name  the name of the signal
	 * @param width the width of the signal
	 */	
	void addSignal(const std::string name, const int width=1);

	/** DEPRECATED, use declare() instead
		 Adds a bus signal to the signal list.
	 * Adds a signal of type Signal::wire to the the signal list. 
	 * The signal added by this method has a flag indicating that it is a bus. 
	 * Even if the signal will have width=1, it will be declared as a standard_logic_vector(0 downto 0)
	 * @param name  the name of the signal
	 * @param width the width of the signal
	 */	
	void addSignalBus(const std::string name, const int width=1);
	

	/** DEPRECATED, use declare() instead
		 Adds a delayed signal (without reset) to the signal list.
	 * Adds to the the signal list
	 * one signal of type Signal::wire
	 * and max(0,delay) signals of type Signal::registeredWithoutReset. 
	 * The signal names are: name, name_d, name_d_d .. and so on. 
	 * This method is equivalent to addSignal if delay<=0
	 * @param name  the name of the signal
	 * @param width the width of the signal
	 * @param delay the delay of the signal. The number of register levels that this signal needs to be delayed. If negative, treated as zero (useful for pipeline synchronization)
	 */	
	void addDelaySignal(const std::string name, const int width, const int delay=1);


	/** DEPRECATED, use declare() instead
		 Adds a delayed signal with synchronous reset to the signal list.
	 * Adds to the the signal list
	 * one signal of type Signal::wire
	 * and max(0,delay) signals of type Signal::registeredWithSyncReset. 
	 * The signal names are: name, name_d, name_d_d .. and so on. 
	 * This method is equivalent to addSignal if delay<=0
	 * @param name  the name of the signal
	 * @param width the width of the signal
	 * @param delay the delay of the signal. The number of register levels that this signal needs to be delayed. If negative, treated as zero (useful for pipeline synchronization)
	 */	
	void addDelaySignalSyncReset(const std::string name, const int width, const int delay=1);


	/** DEPRECATED, use declare() instead
		 Adds a delayed signal bus with synchronous reset to the signal list.
	 * Adds to the the signal list
	 * one signal of type Signal::wire
	 * and max(0,delay) signals of type Signal::registeredWithSyncReset. 
	 * The signal names are: name, name_d, name_d_d .. and so on. 
	 * Each signal added by this method has a flag indicating that it is a bus. 
	 * Even if the signal will have width=1, it will be declared as a standard_logic_vector(0 downto 0)
	 * This method is equivalent to addSignalBus if delay<=0
	 * @param name  the name of the signal
	 * @param width the width of the signal
	 * @param delay the delay of the signal. The number of register levels that this signal needs to be delayed. If negative, treated as zero (useful for pipeline synchronization)
	 */	
	void addDelaySignalBus(const std::string name, const int width, const int delay=1);


	/** DEPRECATED, use declare() instead
		 Adds a delayed signal bus with synchronous reset to the signal list.
	 * Adds to the the signal list
	 * one signal of type Signal::wire
	 * and max(0,delay) signals of type Signal::registeredWithSyncReset. 
	 * The signal names are: name, name_d, name_d_d .. and so on. 
	 * Each signal added by this method has a flag indicating that it is a bus. 
	 * Even if the signal will have width=1, it will be declared as a standard_logic_vector(0 downto 0)
	 * This method is equivalent to addSignalBus if delay<=0
	 * @param name  the name of the signal
	 * @param width the width of the signal
	 * @param delay the delay of the signal. The number of register levels that this signal needs to be delayed. If negative, treated as zero (useful for pipeline synchronization)
	 */	
	void addDelaySignalBusSyncReset(const std::string name, const int width, const int delay=1);
	
	
	/** DEPRECATED, use use() instead
		 Returns the name of a signal at a certain delay.
	 * Returns a string of the form name_d_d_d..._d where #(_d)=delay
	 * @param name  the name of the signal
	 * @param delay the delay of the signal. The number of register levels that this signal needs to be delayed with.
	 If delay<=0 then no delay is inserted. This way the following code synchronizes signals from two paths with different delays:
	 getDelaySignalName(s1,  s2_delay - s1_delay)  // will be delayed if s2_delay>s1_delay
    getDelaySignalName(s2,  s1_delay - s2_delay)  // will be delayed if s1_delay>s2_delay

	 If the operator is not sequential, the string returned is simply
	 name. In principle, the code for a sequential operator is thus
	 gracefully degraded into combinatorial code. See FPAdder for an example.
	 */	 
	string  delaySignal(const string name, const int delay=1);


protected:    
	Target*             target_;          /**< The target on which the operator will be deployed */
	string              uniqueName_;      /**< By default, a name derived from the operator class and the parameters */
	vector<Signal*>     ioList_;          /**< The list of I/O signals of the operator */
	vector<Signal*>     testCaseSignals_; /**< The list of pointers to the signals in a test case entry. Its size also gives the dimension of a test case */
	vector<Signal*>     signalList_;      /**< The list of internal signals of the operator */
	map<string, Operator*> subComponents_;/**< The list of sub-components */
	map<string, string> portMap_;         /**< Port map for an instance of this operator */
	map<string, double> outDelayMap;      /**< Slack delays on the outputs */
	ostringstream       vhdl;             /**< The internal stream to which the constructor will build the VHDL code */
	string                 srcFileName;                /**< used to debug and report.  */
private:
	int                    numberOfInputs_;             /**< The number of inputs of the operator */
	int                    numberOfOutputs_;            /**< The number of outputs of the operator */
	bool                   isSequential_;               /**< True if the operator needs a clock signal*/
	int                    pipelineDepth_;              /**< The pipeline depth of the operator. 0 for combinatorial circuits */
	map<string, Signal*>   signalMap_;                  /**< A container of tuples for recovering the signal based on it's name */ 
	map<string, pair<string, mpz_class> > constants_;    /**< The list of constants of the operator: name, <type, value> */
	map<string, string>    attributes_;                  /**< The list of attribute declarations (name, type) */
	map<pair<string,string>, string >  attributesValues_;/**< attribute values <attribute name, object (component, signal, etc)> ,  value> */
	bool                   hasRegistersWithoutReset_;   /**< True if the operator has registers without a reset */
	bool                   hasRegistersWithAsyncReset_; /**< True if the operator has registers having an asynch reset */
	bool                   hasRegistersWithSyncReset_;  /**< True if the operator has registers having a synch reset */
	string                 commentedName_;              /**< Usually is the default name of the architecture.  */
	string                 copyrightString_;            /**< Authors and years.  */
	int                    currentCycle_;               /**< The current cycle, when building a pipeline */
	double                 criticalPath_;               /**< The current delay of the current pipeline stage */

};

} //namespace
#endif
