#pragma once
#include <algorithm>
#include <array>
#include <random>
#include <vector>
struct SnakeGame {
 std::vector<int> body;int food=-1,direction=1,nextDirection=1,score=0;bool paused=true,over=false,won=false,turned=false;std::mt19937 rng{std::random_device{}()};
 void placeFood(){std::vector<int> empty;for(int i=0;i<400;i++)if(std::find(body.begin(),body.end(),i)==body.end())empty.push_back(i);if(empty.empty()){food=-1;over=true;won=true;return;}food=empty[std::uniform_int_distribution<int>(0,(int)empty.size()-1)(rng)];}
 void reset(){body={210,209,208};direction=nextDirection=1;score=0;paused=true;over=won=turned=false;placeFood();}
 void turn(int d){if(over||turned||d==(direction+2)%4)return;nextDirection=d;turned=true;paused=false;}
 void tick(){if(paused||over)return;direction=nextDirection;turned=false;int x=body.front()%20,y=body.front()/20;x+=direction==1?1:direction==3?-1:0;y+=direction==2?1:direction==0?-1:0;if(x<0||x>=20||y<0||y>=20){over=true;return;}int head=y*20+x;bool eating=head==food;auto end=body.end()-(eating?0:1);if(std::find(body.begin(),end,head)!=end){over=true;return;}body.insert(body.begin(),head);if(eating){score+=10;placeFood();}else body.pop_back();}
};
struct MemoryGame {
 std::array<int,16> cards{};std::array<bool,16> matched{};int first=-1,second=-1,attempts=0,pairs=0,focus=0;std::mt19937 rng{std::random_device{}()};
 void reset(){for(int i=0;i<16;i++)cards[i]=i/2+1;std::shuffle(cards.begin(),cards.end(),rng);matched.fill(false);first=second=-1;attempts=pairs=focus=0;}
 bool flip(int i){if(i<0||i>=16||matched[i]||second>=0||first==i)return false;if(first<0){first=i;return false;}second=i;attempts++;if(cards[first]==cards[second]){matched[first]=matched[second]=true;pairs++;first=second=-1;return false;}return true;}
 void conceal(){first=second=-1;}
};
