#pragma once
#include <array>
#include <random>
struct Game2048 {
 std::array<int,16> cells{}; unsigned long long score=0; std::mt19937 rng{std::random_device{}()};
 void spawn(){std::vector<int> free;for(int i=0;i<16;i++)if(!cells[i])free.push_back(i);if(!free.empty())cells[free[std::uniform_int_distribution<int>(0,(int)free.size()-1)(rng)]]=std::uniform_int_distribution<int>(0,9)(rng)?2:4;}
 void reset(){cells.fill(0);score=0;spawn();spawn();}
 bool move(int direction,bool add=true){auto previous=cells;for(int line=0;line<4;line++){std::vector<int> values;std::array<int,4> indices{};for(int k=0;k<4;k++){int index=direction==0?k*4+line:direction==1?line*4+3-k:direction==2?(3-k)*4+line:line*4+k;indices[k]=index;if(cells[index])values.push_back(cells[index]);}std::vector<int> merged;for(size_t k=0;k<values.size();k++){int value=values[k];if(k+1<values.size()&&value==values[k+1]){value*=2;score+=value;k++;}merged.push_back(value);}for(int k=0;k<4;k++)cells[indices[k]]=k<(int)merged.size()?merged[k]:0;}bool changed=cells!=previous;if(changed&&add)spawn();return changed;}
 bool over()const{for(int i=0;i<16;i++)if(!cells[i]||(i%4<3&&cells[i]==cells[i+1])||(i<12&&cells[i]==cells[i+4]))return false;return true;}
 bool won()const{return *std::max_element(cells.begin(),cells.end())>=2048;}
};
inline bool shouldOfferGame(bool scanning,bool cancelled,bool offered,unsigned long long elapsed){return scanning&&!cancelled&&!offered&&elapsed>=30000;}
