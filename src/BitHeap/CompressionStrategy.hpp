#ifndef COMPRESSIONSTRATEGY_HPP
#define COMPRESSIONSTRATEGY_HPP


#include <vector>
#include <sstream>

#include "Operator.hpp"

#include "BitHeap/Bit.hpp"
#include "BitHeap/Compressor.hpp"
#include "BitHeap/BitheapNew.hpp"
#include "BitHeap/BitheapPlotter.hpp"

#include "IntAddSubCmp/IntAdder.hpp"

namespace flopoco
{

class Bit;
class Compressor;
class BitheapNew;
class IntAdder;
class BitheapPlotter;

	class CompressionStrategy
	{
	public:

		/**
		 * A basic constructor for a compression strategy
		 */
		CompressionStrategy(BitheapNew *bitheap);

		/**
		 * Destructor
		 */
		~CompressionStrategy();


		/**
		 * Start compressing the bitheap
		 */
		void startCompression();

	protected:

		/**
		 * @brief start a new round of compression of the bitheap using compressors
		 *        only bits that are within at most a given delay from the soonest
		 *        bit are compressed
		 * @param delay the maximum delay between the compressed bits and the soonest bit
		 * @return whether a compression has been performed
		 * @param soonestCompressibleBit the earliest bit in the bitheap that is compressible
		 */
		bool compress(double delay, Bit *soonestCompressibleBit);

		/**
		 * @brief apply a compressor to the column given as parameter
		 * @param bitVector the bits to compress
		 * @param compressor the compressor used for compression
		 * @param weight the weight of the lsb column of bits
		 */
		void applyCompressor(vector<Bit*> bitVector, Compressor* compressor, int weight);

		/**
		 * @brief applies an adder with wIn = msbColumn-lsbColumn+1;
		 * lsbColumn has size=3, if hasCin=true, and the other columns (including msbColumn) have size=2
		 * @param msbColumn the msb of the addends
		 * @param lsbColumn the lsb of the addends
		 * @param hasCin whether the lsb column has size 3, or 2
		 */
		void applyAdder(int msbColumn, int lsbColumn, bool hasCin = true);

		/**
		 * @brief compress the remaining columns using adders
		 */
		void applyAdderTreeCompression();

		/**
		 * @brief generate the final adder for the bit heap
		 * (when the columns height is maximum 2/3)
		 * @param isXilinx whether the target FPGA is a Xilinx device
		 *        (different primitives used)
		 */
		void generateFinalAddVHDL(bool isXilinx = true);

		/**
		 * @brief computes the latest bit from the bitheap, between columns lsbColumn and msbColumn
		 * @param msbColumn the weight of the msb column
		 * @param lsbColumn the weight of the lsb column
		 */
		Bit* getLatestBit(unsigned lsbColumn, unsigned msbColumn);

		/**
		 * @brief computes the soonest bit from the bitheap, between columns lsbColumn and msbColumn
		 *        if the bitheap is empty (or no bits are available for compression), nullptr is returned
		 * @param msbColumn the weight of the msb column
		 * @param lsbColumn the weight of the lsb column
		 */
		Bit* getSoonestBit(unsigned lsbColumn, unsigned msbColumn);

		/**
		 * @brief computes the soonest compressible bit from the bitheap, between columns lsbColumn and msbColumn
		 *        if the bitheap is empty (or no bits are available for compression), nullptr is returned
		 * @param msbColumn the weight of the msb column
		 * @param lsbColumn the weight of the lsb column
		 * @param delay the maximum delay between the compressed bits
		 */
		Bit* getSoonestCompressibleBit(unsigned lsbColumn, unsigned msbColumn, double delay);

		/**
		 * @brief can the compressor given as parameter be applied to the column
		 *        given as parameter, compressing bits within the given delay from
		 *        the soonest bit, given as parameter
		 * @param columnNumber the column being compressed (column index)
		 * @param compressor the compressor being used for compression (compressor index)
		 * @param soonestBit the bit used as reference for timing
		 * @param delay the maximum delay between the bits compressed and the reference bit
		 * @return a vector containing the bits that can be compressed, or an empty vector
		 *         if the compressor cannot be applied
		 */
		vector<Bit*> canApplyCompressor(unsigned columnNumber, unsigned compressorNumber, Bit* soonestBit, double delay);

		/**
		 * @brief generate all the compressors that will be used for
		 * compressig the bitheap
		 */
		void generatePossibleCompressors();

		/**
		 * @brief return the delay of a compression stage
		 * (there can be several compression staged in a cycle)
		 */
		double getCompressorDelay();

		/**
		 * @brief verify up until which lsb column the compression
		 * has already been done and concatenate those bits
		 */
		void concatenateLSBColumns();

		/**
		 * @brief concatenate all the chunks of the compressed bitheap and create the final result
		 */
		void concatenateChunks();

	private:
		BitheapNew *bitheap;                        /**< The bitheap this compression strategy belongs to */
		BitheapPlotter *bitheapPlotter;             /**< The bitheap plotter for this bitheap compression */

		vector<Compressor*> possibleCompressors;    /**< All the possible compressors that can be used for compressing the bitheap */

		unsigned compressionDoneIndex;              /**< The index in the range msb-lsb up to which the compression is completed */
		vector<string> chunksDone;                  /**< The vector containing the chunks of the compression result */

		int stagesPerCycle;                         /**< The number of stages of compression in each cycle */
		double compressionDelay;                    /**< The duration of a compression stage */

		// For error reporting to work
		int guid;
		string srcFileName;
		string uniqueName_;
	};
}

#endif
