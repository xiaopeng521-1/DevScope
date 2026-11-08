#include "mini_games.h"
#include <cassert>
#include <iostream>
int main(){SnakeGame s;s.reset();assert(s.paused&&s.body.size()==3);assert(std::find(s.body.begin(),s.body.end(),s.food)==s.body.end());s.food=211;s.turn(1);s.tick();assert(s.score==10&&s.body.size()==4&&s.body.front()==211);s.turn(3);assert(!s.turned);s.turn(0);s.turn(3);assert(s.nextDirection==0);s.tick();assert(s.body.front()==191);
s.body={19,18,17};s.direction=s.nextDirection=1;s.paused=false;s.over=false;s.tick();assert(s.over);
s.reset();s.body={21,22,42,41};s.direction=3;s.nextDirection=2;s.food=399;s.paused=false;s.tick();assert(!s.over&&s.body.front()==41);
MemoryGame m;m.reset();for(int value=1;value<=8;value++)assert(std::count(m.cards.begin(),m.cards.end(),value)==2);for(int i=0;i<16;i++)m.cards[i]=i/2+1;assert(!m.flip(0));assert(!m.flip(0)&&m.attempts==0);assert(!m.flip(1)&&m.pairs==1&&m.matched[0]);assert(!m.flip(2));assert(m.flip(4));assert(!m.flip(6)&&m.second==4);m.conceal();assert(m.first==-1&&m.second==-1);for(int i=2;i<16;i+=2){m.flip(i);m.flip(i+1);}assert(m.pairs==8);std::cout<<"PASS: snake collision/growth/turn guards and memory matching/cooldown/win\n";}
