
#include "ParandehAfsharCompressionStrategy.hpp"
//#include "CompressionStrategy.hpp"
//#include "BitHeap/BitheapNew.hpp"


using namespace std;

namespace flopoco{


	ParandehAfsharCompressionStrategy::ParandehAfsharCompressionStrategy(BitheapNew* bitheap) : CompressionStrategy(bitheap)
	{

	}




	void ParandehAfsharCompressionStrategy::compressionAlgorithm()
	{
		REPORT(DEBUG, "compressionAlgorithm is ParandehAfshar");

		//for the parandehAfshar algorithm, the compressors should be ordered by efficiency
		orderCompressorsByCompressionEfficiency();

		//adds the Bits to stages and columns
		orderBitsByColumnAndStage();

		//populates bitAmount. on this simple structure the parandehAfshar algorithm is working
		fillBitAmounts();

		//prints out how the inputbits of the bitheap looks like
		printBitAmounts();

		//new solution
		solution = Solution();

		//parandehAfshar generates the compressor tree but only works on the bitAmount datastructure and fills the solution. No VHDL-Code is written here.
		parandehAfshar();

		//here the VHDL-Code for the compressors as well as the bits->compressors->bits are being written.
		applyAllCompressorsFromSolution();

	}


	void ParandehAfsharCompressionStrategy::parandehAfshar(){
		REPORT(DEBUG, " in parandehAfsahr algorithm");
		double threshold = 0.0;

		bool exit = false;
		unsigned int s = 0;
		while(true){

			//make sure that there are s+1 stages to put compressors into stage s with
			//outputs at at least stage s+1
			while(bitAmount.size() < s + 1 + 1){
				bitAmount.resize(s + 1 + 1);
				bitAmount[s + 1].resize(bitAmount[s].size(), 0);
			}

			bool found = true;
			while(found == true){
				found = false;

				BasicCompressor* compressor = nullptr;
				unsigned int column = 0;
				pair<BasicCompressor*, int> result;


				bool used[bitAmount[s].size()];
				for(unsigned int k = 0; k < bitAmount[s].size(); k++){
					used[k] = 0;
				}

				//first the the highest, then the second highest column ...
				//so in the worst case we have to check bitAmount[s].size() many
				for(unsigned int a = 0; a < bitAmount[s].size(); a++){
					unsigned int currentMaxColumn = 0;
					int maxSize = 0;

					//find max column which wasn't used in previous iteration
					for(unsigned int c = 0; c < bitAmount[s].size(); c++){
						if(used[c] == false && bitAmount[s][c] > maxSize){
							currentMaxColumn = c;
							maxSize = bitAmount[s][c];
						}
					}
					used[currentMaxColumn] = true;
					//cout << "currentMaxColumn is " << currentMaxColumn << " and maxSize is " << maxSize << endl;
					if(maxSize > 0){
						pair<BasicCompressor*, int> tempResult = ParandehAfsharSearch(s, currentMaxColumn);
						//cout << "compressor is " << tempResult.first << " and middlePart is " << tempResult.second << endl;
						if(tempResult.first != nullptr && tempResult.second >= 0){
							found = true;
							result = tempResult;
							REPORT(DEBUG, " found compressor");
						}
					}
					if(found){
						break;
					}
				}

				if(found){
					column = result.second;
					compressor = result.first;
					placeCompressor(s, column, compressor);
					printBitAmounts();
					REPORT(DEBUG, "placed compressor " << compressor << " at stage " << s << " and column " << column);
				}

			}
			REPORT(DEBUG, "finished stage " << s << " with parandeh-afhar algorithm.");

			//finished one stage. bring the remaining bits in bitAmount to the new stage
			for(unsigned int c = 0; c < bitAmount[s].size(); c++){
				if(bitAmount[s][c] > 0){
					bitAmount[s + 1][c] += bitAmount[s][c];
					bitAmount[s][c] = 0;
				}
			}

			//check if we are finished
			bool finished = checkAlgorithmReachedAdder(2, s + 1); //check the next stage.
			printBitAmounts();
			if(finished){
				break;
			}

			//new stage to bitAmount
			bitAmount.resize(bitAmount.size() + 1);
			//new stage has as many columns as previous stage
			bitAmount[bitAmount.size() - 1].resize(bitAmount[bitAmount.size() - 2].size());
			s++;
		}

		REPORT(DEBUG, "finished parandehAfshar algorithm");
	}


	pair<BasicCompressor*, int> ParandehAfsharCompressionStrategy::ParandehAfsharSearch(unsigned int stage, unsigned int column){
		BasicCompressor* compressor = nullptr;
		int resultColumn = -1;
		double achievedEfficiencyBest = -1.0;
		bool found = false;

		for(unsigned int i = 0; i < possibleCompressors.size(); i++){
			double achievedEfficiencyCurrentLeft = getCompressionEfficiency(stage, column, possibleCompressors[i]);
			double achievedEfficiencyCurrentRight = -1.0;
			//cout << "column is " << column << " and possibleCompressors[i]->getHeights is " << possibleCompressors[i]->getHeights() << endl;
			int rightStartPoint = column - (possibleCompressors[i]->getHeights() - 1);
			//cout << "right StartPoint is " << rightStartPoint << endl;
			if(rightStartPoint >= 0){
				achievedEfficiencyCurrentRight = getCompressionEfficiency(stage, rightStartPoint, possibleCompressors[i]);
			}

			if(achievedEfficiencyCurrentLeft > 0.0001 && achievedEfficiencyCurrentRight <= achievedEfficiencyCurrentLeft + 0.0001){
				//normal (left) search is successfull - prefer it if left and right search has equal efficiancy. if rightEfficiency > leftEfficiency, prefer right.
				if(achievedEfficiencyBest + 0.0001 < achievedEfficiencyCurrentLeft){
					achievedEfficiencyBest = achievedEfficiencyCurrentLeft;
					compressor = possibleCompressors[i];
					resultColumn = column;
				}
				found = true; //achievedEfficiencyCurrentLeft > 0
			}
			else if(achievedEfficiencyCurrentRight > 0.0001){
				//right search is successful and efficiency is bigger than left search

				if(achievedEfficiencyBest + 0.0001 < achievedEfficiencyCurrentRight){
					achievedEfficiencyBest = achievedEfficiencyCurrentRight;
					compressor = possibleCompressors[i];
					resultColumn = rightStartPoint;
				}
				found = true; //achievedEfficiencyCurrentRight > 0
			}
		}

		pair<BasicCompressor*, int> result;
		if(found == true){
			result.first = compressor;
			result.second = resultColumn;
			cout << "returning compressor " << compressor->getStringOfIO() << " and resultColumn " << resultColumn << " with achievedEfficiencyBest is " << achievedEfficiencyBest << endl;
		}
		else{
			result.first = nullptr;
			result.second = -1;
			cout << "found no compressor " << endl;
		}
		return result;

	}


}