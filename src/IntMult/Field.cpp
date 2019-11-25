#include "Field.hpp"
#include <iostream>

namespace flopoco {
    Field::Field(unsigned int wX_, unsigned int wY_) : wX(wX_), wY(wY_), missing(wX_ * wY_) {
        field.resize(wY);
        for(unsigned int i = 0; i < wY; i++) {
            field[i].resize(wX);
            for(unsigned int j = 0; j < wX; j++) {
                field[i][j] = false;
            }
        }

        setCursor(0, 0);
    }

    Field::Field(const Field &copy) {
        wX = copy.wX;
        wY = copy.wY;
        missing = copy.missing;
        cursor =  pair<unsigned int, unsigned int>(copy.cursor);

        field.resize(wY);
        for(unsigned int i = 0; i < wY; i++) {
            field[i].resize(wX);
            for(unsigned int j = 0; j < wX; j++) {
                field[i][j] = copy.field[i][j];
            }
        }
    }

    Field::~Field() {
        field.clear();
    }

    void Field::reset() {
        for(unsigned int i = 0; i < wY; i++) {
            for(unsigned int j = 0; j < wX; j++) {
                field[i][j] = false;
            }
        }

        setCursor(0, 0);
        missing = wX * wY;
    }

    unsigned int Field::checkTilePlacement(const pair<unsigned int, unsigned int> coord, const BaseMultiplierParametrization& tile) {
        unsigned int endX = coord.first + tile.getTileXWordSize();
        unsigned int endY = coord.second + tile.getTileYWordSize();
        unsigned int maxX = endX > wX ? wX : endX;
        unsigned int maxY = endY > wY ? wY : endY;

        unsigned int covered = 0;

        for(unsigned int i = coord.second; i < maxY; i++) {
            for(unsigned int j = coord.first; j < maxX; j++) {
                //check if tile could cover this area
                if(!tile.shapeValid(j - coord.first, i - coord.second)) {
                    cout << "coord" << j << " " << i << " is not covered!" << endl;
                    continue;
                }

                if(field[i][j]) {
                    return 0;
                }

                covered++;
            }
        }

        cout << "Trying to place tile with size " << tile.getTileXWordSize() << " " << tile.getTileYWordSize() << " at position " << coord.first << " " << coord.second << endl;
        cout << "It would cover " << covered << endl;

        return covered;
    }

    pair<unsigned int, unsigned int> Field::placeTileInField(const pair<unsigned int, unsigned int> coord, const BaseMultiplierParametrization& tile) {
        unsigned int endX = coord.first + tile.getTileXWordSize();
        unsigned int endY = coord.second + tile.getTileYWordSize();
        unsigned int maxX = endX > wX ? wX : endX;
        unsigned int maxY = endY > wY ? wY : endY;

        for(unsigned int i = coord.second; i < maxY; i++) {
            for (unsigned int j = coord.first; j < maxX; j++) {
                //check if tile could cover this area and if area is free
                if (tile.shapeValid(j - coord.first, i - coord.second) && !field[i][j]) {
                    missing--;
                    field[i][j] = true;
                }
            }
        }


        //printField();
        updatePosition();

        cout << "Placed tile with size " << tile.getTileXWordSize() << " " << tile.getMultYWordSize() << " at position " << coord.first << " " << coord.second << " new coord is " << cursor.first << " " << cursor.second << endl;

        return cursor;
    }

    void Field::updatePosition() {
        while(cursor.second != wY) {
            //try to find next free position in the current line
            for(unsigned int i = 0; i < wX; i++) {
                if(!field[cursor.second][i]) {
                    cursor.first = i;
                    return;
                }
            }

            //no free position in this line
            cursor.second++;
        }

        cursor.first = 0;
        cursor.second = 0;
    }

    bool Field::isFull() {
        return missing == 0;
    }

    unsigned int Field::getMissing() {
        return missing;
    }

    unsigned int Field::getMissingLine() {
        unsigned int missing = 0;

        for(unsigned int i = cursor.first; i < wX; i++) {
            if(field[cursor.second][i]) {
                break;
            }
            missing++;
        }

        return missing;
    }

    unsigned int Field::getMissingHeight() {
        unsigned int missing = 0;

        for(unsigned int i = cursor.second; i < wY; i++) {
            if(field[i][cursor.first]) {
                break;
            }
            missing++;
        }

        return missing;
    }

    pair<unsigned int, unsigned int> Field::getCursor() {
        return cursor;
    }

    void Field::setCursor(unsigned int x, unsigned int y)  {
        if(x > wX) {
            x = 0;
        }

        if(y > wY) {
            y = 0;
        }

        cursor.first = x;
        cursor.second = y;
    }

    void Field::printField() {
        //TODO: mirror output
        for(auto v: field) {
            for(auto c: v) {
                cout << c;
            }
            cout << endl;
        }
    }
}
