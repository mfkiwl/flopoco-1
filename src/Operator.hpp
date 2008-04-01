#ifndef OPERATOR_HPP
#define OPERATOR_HPP
#include<vector>
#include <gmpxx.h>
//#include "cpphdl/signal.hh"
#include "Target.hpp"

using namespace std;

// variables set by the command-line interface in main.cpp

extern  int verbose;

const std::string tab = "   ";




class Operator
{
public:
  /**
     A test case consists of one input and a vector of possible outputs.
     
     The input is a vector of integers which will be converted to
     std_logic_vectors, one for each input declared by add_input or
     add_fp_input.
     
     The expected_output is a vector with as many entries
     as there are outputs of the entity. Each entry may consist of
     several possible outputs. The test will succeed if each actual
     output corresponds to one of the elements of the corresponding list.
  */
  struct TestCase{
    vector<mpz_class> input;
    vector<vector<mpz_class> > expected_output; 
  };


  // A local class to be replaced by the cpphdl version some day
  class Signal{
  public:
    typedef enum{in,out,wire,registered,registered_with_async_reset,registered_with_sync_reset } type_t;
    
    Signal(const std::string name, const type_t type, const int width = 1): 
      _name(name), _type(type), _wE(0), _wF(0), _width(width){
      isFP = false;
    }

    Signal(const std::string name, const type_t type, const int wE, const int wF): 
      _name(name), _type(type), _wE(wE), _wF(wF), _width(wE+wF+3){
      isFP = true;
    }
    
    ~Signal(){}
    
    std::string id(){return _name;}
    
    int width(){return _width;}
    int wE(){return(_wE);}
    int wF(){return(_wF);}
    
    type_t type() {return _type;}
    
    std::string toVHDL() {
      std::ostringstream o; 
      if(type()==Signal::wire || type()==Signal::registered || type()==Signal::registered_with_async_reset || type()==Signal::registered_with_sync_reset) 
	o << "signal ";
      o << id();
//       if(type()==Signal::registered || type()==Signal::registered_with_reset|| type()==Signal::delay)
// 	o << ", " << id() << "_d";	 
      o << " : ";
      if(type()==Signal::in)
	 o << "in ";
      if(type()==Signal::out)
	 o << "out ";

      if (1==width()) 
	o << "std_logic" ;
      else 
	if(isFP) 
	  o << " std_logic_vector(" << wE() <<"+"<<wF() << "+2 downto 0)";
	else
	  o << " std_logic_vector(" << width()-1 << " downto 0)";
      return o.str();
    }

  private:
    const std::string _name;
    const type_t _type;
    const uint32_t _width;
    bool isFP;
    const uint32_t _wE;  // used only for FP signals
    const uint32_t _wF;  // used only for FP signals
  };



  Operator()  {
    number_of_inputs=0;
    number_of_outputs=0;
    has_registers=false;
    has_registers_with_async_reset=false;
    has_registers_with_sync_reset=false;
  }

  Operator(Target* target_)  {
    target=target_;
    number_of_inputs=0;
    number_of_outputs=0;
    has_registers=false;
    has_registers_with_async_reset=false;
    has_registers_with_sync_reset=false;
  }
  
  virtual ~Operator() {}



  /** The following functions should be used to declare input and output signals,
   excluding clk and rst */

  void add_input(const std::string name, const int width=1);
  void add_output(const std::string name, const int width=1);
  
  // equivalent to the previous for FP signals 
  void add_FP_input(const std::string name, const int wE, const int wF);
  void add_FP_output(const std::string name, const int wE, const int wF);
  
  /** The following should be used to declare standard signals */
  void add_signal(const std::string name, const int width=1);
  
  /** The following should be used to declare registered signals: it
      will declare name and name_d, and output_vhdl_registers will
      build the registers between them. */
  void add_registered_signal(const std::string name, const int width=1);
  void add_registered_signal_with_async_reset(const std::string name, const int width=1);
  void add_registered_signal_with_sync_reset(const std::string name, const int width=1);

  /** The following adds a signal, and also a shift register on it of depth delay.
      There will be depth levels of registers, named name_d, name_d_d and so on.
      The string returned is the name of the last signal.
    */
  string add_delay_signal(const std::string name, const int width, const int delay);
  /** An helper function that return the name of an intermediate signal generated by add_delay_signal*/
  string  get_delay_signal_name(const string name, const int delay);

  void output_vhdl_registers(std::ostream& o);


  // Helper functions for VHDL output
  
  /** Outputs component declaration */
  virtual void output_vhdl_component(std::ostream& o, std::string name);

  void output_vhdl_component(std::ostream& o);  // calls the previous with name = unique_name
  

  // Output the licence
  void Licence(std::ostream& o, std::string authorsyears);
  
  // output the standard library paperwork
  static void StdLibs(std::ostream& o){
    o<<"library ieee;\nuse ieee.std_logic_1164.all;"<<endl 
     <<"use ieee.std_logic_arith.all;"<<endl
     <<"use ieee.std_logic_unsigned.all;"<<endl 
     <<"library work;"<<endl<<endl;
  };
    
  // output the identity 
  void output_vhdl_entity(std::ostream& o);

  // output all the signal declarations 
  void output_vhdl_signal_declarations(std::ostream& o);

  // the main function outputs the VHDL for the operator

  virtual void output_vhdl(std::ostream& o, std::string name) =0 ;
  void output_vhdl(std::ostream& o);   // calls the previous with name = unique_name



  // Functions related to pipelining (work in progress)
  
  bool is_sequential();  // true if the operator needs a clock signal. It will also get a rst but doesn't need to use it
  void set_sequential(); 
  void set_combinatorial();
  int pipeline_depth();
  void increment_pipeline_depth();
  void set_pipeline_depth(int d);



  // Functions related to test case generation (work in progress)

  /** Add a test case. 
  */
  void add_test_case(TestCase t);
  
  /** Add all the possible standard test cases to the test
      list. Standard test cases test behaviour on zero, infinities
      and other special values. The designer of an operator should
      define this method. */
  void add_standard_test_cases();
  
  /** Add n random test cases to the test list. The designer of an
      operator should define this method. */
  void add_random_test_cases(int n);
  

  /** Applies a reset to the operator */
  void reset_test();
  
  /** Combine the test vectors into a VHDL test bench that takes into account the pipeline structure.  */
  virtual void output_vhdl_test(std::ostream& o);
  
  /**Final report function, prints to the terminal.  By default
     reports the pipeline depth, but feel free to overload if you have
     anything useful to tell to the end user
  */
  virtual void output_final_report();


  string unique_name;
  

    
  

  
  vector<Signal*> ioList; 
  vector<Signal*> signalList; 
//   vector<Signal*> registerList; 
//   vector<Signal*> register_with_resetList; 

protected:    

  Target* target;

private:
  int number_of_inputs;
  int number_of_outputs;
  bool _is_sequential;  // true if the operator needs a clock signal. It will also get a rst but doesn't need to use it
  int _pipeline_depth;

  vector<TestCase> test_case_list;

  bool has_registers;
  bool has_registers_with_async_reset;
  bool has_registers_with_sync_reset;

};


#endif
