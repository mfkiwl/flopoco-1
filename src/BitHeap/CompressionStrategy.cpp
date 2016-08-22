
#include "CompressionStrategy.hpp"


using namespace std;

namespace flopoco{


	CompressionStrategy::CompressionStrategy(BitheapNew *bitheap_) :
		bitheap(bitheap_)
	{
		stringstream s;

		 //for REPORT to work
		srcFileName = bitheap->op->getSrcFileName() + ":BitHeap:CompressionStrategy";
		guid = Operator::getNewUId();
		s << bitheap->name << "_CompressionStrategy_" << guid;
		uniqueName_ = s.str();

		compressionDoneIndex = 0;

		compressionDelay = bitheap->op->getTarget()->lutDelay() + bitheap->op->getTarget()->localWireDelay();
		stagesPerCycle = (1.0/bitheap->op->getTarget()->frequency()) / compressionDelay;

		generatePossibleCompressors();
	}


	CompressionStrategy::~CompressionStrategy(){
	}


	void CompressionStrategy::startCompression()
	{
		bool bitheapCompressed = true;
		double delay;

		//first, set the delay to the delay of a compressor
		delay = compressionDelay;

		//compress the bitheap to height 2
		//	(the last column can be of height 3)
		if(!bitheap->isCompressed || bitheap->compressionRequired())
		{
			while(bitheap->compressionRequired())
			{
				bitheapCompressed = compress(delay);
				bitheap->removeCompressedBits();

				if(bitheapCompressed == false)
				{
					if(delay == compressionDelay)
						delay = 1.0 / bitheap->op->getTarget()->frequency();
					else
						delay += 1.0 / bitheap->op->getTarget()->frequency();
				}
			}
		}else{
			REPORT(DEBUG, "Bitheap already compressed, so startCompression has nothing else to do.");
		}
		//generate the final addition
		generateFinalAddVHDL(bitheap->op->getTarget()->getVendor() == "Xilinx");
		//mark the bitheap as compressed
		bitheap->isCompressed = true;
	}


	bool CompressionStrategy::compress(double delay)
	{
		Bit* soonestBit = nullptr;
		bool compressionPerformed = false;

		//get the bit with the smallest (cycle, critical path)
		soonestBit = getSoonestBit(0, bitheap->size-1);
		//if there is no soonest bit, the ther's nothing else to do
		if(soonestBit == nullptr)
			return false;

		//try to apply the compressors in the decreasing
		//	order of their compression ratio
		for(unsigned i=0; i<possibleCompressors.size(); i++)
		{
			//go through all the lines and try to apply the compressor
			//	to bits that are within a delay of a compressor
			for(unsigned j=0; j<bitheap->bits.size(); j++)
			{
				vector<Bit*> compressorBitVector = canApplyCompressor(j, i, soonestBit, compressionDelay);

				while(compressorBitVector.size() > 0)
				{
					applyCompressor(j, i);
					compressorBitVector = canApplyCompressor(j, i, soonestBit, compressionDelay);
					compressionPerformed = true;
				}
			}
		}

		return compressionPerformed;
	}


	Bit* CompressionStrategy::getSoonestBit(unsigned lsbColumn, unsigned msbColumn)
	{
		Bit* soonestBit = nullptr;
		unsigned count = lsbColumn;

		if((lsbColumn < bitheap->lsb) || (msbColumn > bitheap->msb))
			THROWERROR("Invalid arguments for getSoonest bit: lsbColumn="
					<< lsbColumn << " msbColumn=" << msbColumn);
		if(bitheap->getMaxHeight() == 0)
			REPORT(DEBUG, "Warning: trying to obtain the soonest bit from an empty bitheap!");

		//determine the first non-empty bit column
		while((count < msbColumn) && (bitheap->bits[count].size() == 0))
			count++;
		//select the first bit from the column
		soonestBit = bitheap->bits[count][0];

		//determine the soonest bit
		for(unsigned i=count; i<=msbColumn; i++)
			for(unsigned j=0; j<bitheap->bits[i].size(); j++)
			{
				Bit* currentBit = bitheap->bits[i][j];

				//only consider bits that are available for compression
				if(currentBit->type != Bit::BitType::free)
					continue;

				if((soonestBit->signal->getCycle() > currentBit->signal->getCycle()) ||
						((soonestBit->signal->getCycle() == currentBit->signal->getCycle())
								&& (soonestBit->signal->getCriticalPath() > currentBit->signal->getCriticalPath())))
				{
					soonestBit = currentBit;
				}
			}

		return soonestBit;
	}


	vector<Bit*> CompressionStrategy::canApplyCompressor(unsigned columnNumber, unsigned compressorNumber, Bit* soonestBit, double delay)
	{
		Compressor *compressor;
		vector<Bit*> returnValue;
		double soonestBitTotalTime = 0.0;

		if(compressorNumber > possibleCompressors.size())
		{
			THROWERROR("Compressor index out of range: compressorNumber="
					<< compressorNumber << " maximum index=" << possibleCompressors.size()-1);
		}else{
			compressor = possibleCompressors[compressorNumber];
			soonestBitTotalTime = soonestBit->signal->getCycle() * 1.0/bitheap->op->getTarget()->frequency() +
									soonestBit->signal->getCriticalPath();
		}

		//check there are sufficient columns of bits
		if(compressor->heights.size()+columnNumber >= bitheap->bits.size())
			return returnValue;

		for(unsigned i=0; i<compressor->heights.size(); i++)
		{
			int bitCount = 0;
			int columnIndex = 0;

			while((bitCount < compressor->heights[i])
					&& (columnIndex < bitheap->bits[columnNumber+i].size()))
			{
				Bit *currentBit = bitheap->bits[columnNumber+i][columnIndex];
				double currentBitTotalTime = 0.0;

				//only consider bits that are free
				if(currentBit->type != Bit::BitType::free)
					continue;

				//the total time of the current bit
				currentBitTotalTime = currentBit->signal->getCycle() * 1.0/bitheap->op->getTarget()->frequency() +
										currentBit->signal->getCriticalPath();
				//check if the bit can be compressed
				if(currentBitTotalTime-soonestBitTotalTime <= delay)
				{
					returnValue.push_back(currentBit);
					bitCount++;
				}
				//pass to the next bit in the column
				columnIndex++;
			}
			//if the current column does not contain sufficient bits,
			//	then the compressor cannot be applied
			if(bitCount < compressor->heights[i])
			{
				returnValue.clear();
				return returnValue;
			}
		}

		return returnValue;
	}


	void CompressionStrategy::generatePossibleCompressors()
	{
		int col0, col1;

#if 0 // The generic alternative
		//Generate all "workable" compressors for 2 columns
		for(col0=maxCompressibleBits; col0>=3; col0--)
			for(col1=col0; col1>=0; col1--)
				if((col0 + col1<=maxCompressibleBits) && (intlog2(col0 + 2*col1)<=3))
					{
						vector<int> newVect;

						REPORT(DEBUG, "Generating compressor for col1=" << col1 <<", col0=" << col0);

						newVect.push_back(col0);
						newVect.push_back(col1);
						Compressor* bc = new Compressor(op->getTarget(), newVect);
						possibleCompressors.push_back(bc);
					}

#endif

		//generate the compressors, in decreasing order
		//	of their compression ratio
		{
			vector<int> newVect;

			col0=6;
			col1=0;

			newVect.push_back(col0);
			newVect.push_back(col1);
			possibleCompressors.push_back(new Compressor(bitheap->op->getTarget(), newVect));
		}
		{
			vector<int> newVect;

			col0=4;
			col1=1;

			newVect.push_back(col0);
			newVect.push_back(col1);
			possibleCompressors.push_back(new Compressor(bitheap->op->getTarget(), newVect));
		}
		{
			vector<int> newVect;

			col0=5;
			col1=0;

			newVect.push_back(col0);
			newVect.push_back(col1);
			possibleCompressors.push_back(new Compressor(bitheap->op->getTarget(), newVect));
		}
		{
			vector<int> newVect;

			col0=3;
			col1=1;

			newVect.push_back(col0);
			newVect.push_back(col1);
			possibleCompressors.push_back(new Compressor(bitheap->op->getTarget(), newVect));
		}
		{
			vector<int> newVect;

			col0=4;
			col1=0;

			newVect.push_back(col0);
			newVect.push_back(col1);
			possibleCompressors.push_back(new Compressor(bitheap->op->getTarget(), newVect));
		}
		{
			vector<int> newVect;

			col0=3;
			col1=2;

			newVect.push_back(col0);
			newVect.push_back(col1);
			possibleCompressors.push_back(new Compressor(bitheap->op->getTarget(), newVect));
		}
		{
			vector<int> newVect;

			col0=3;
			col1=0;

			newVect.push_back(col0);
			newVect.push_back(col1);
			possibleCompressors.push_back(new Compressor(bitheap->op->getTarget(), newVect));
		}
	}
}




