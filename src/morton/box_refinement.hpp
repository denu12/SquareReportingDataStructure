#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>

namespace morton_box {
// Retain the MortonPlus initial corner cells, occupancy cutoff, prefix table,
// and refinement depth. The cell test here is exact closed-box geometry.
template<unsigned D, class Key, class Point, class Encode, class Decode, class Lower, class Report>
void refine(Point lo, Point hi, const auto& keys, std::size_t size,
            const auto& lut, unsigned lut_depth, Encode encode, Decode decode, Lower lower, Report report) {
    if(!size)return;
    std::uint32_t width=0;
    for(unsigned a=0;a<D;++a){if(lo[a]>hi[a])return;width=std::max(width,hi[a]-lo[a]);}
    const unsigned depth=std::countl_zero(width),target=std::min(32u,depth+6);
    auto suffix=[](unsigned d){unsigned bits=D*(32-d);return bits==sizeof(Key)*8?~Key(0):(Key(1)<<bits)-1;};
    std::array<Key,1u<<D> cells;
    for(unsigned m=0;m<cells.size();++m){Point corner;for(unsigned a=0;a<D;++a)corner[a]=m&(1u<<a)?hi[a]:lo[a];cells[m]=encode(corner)&~suffix(depth);}
    std::sort(cells.begin(),cells.end());
    std::size_t cursor=0;
    auto visit=[&](auto&& self,Key code,unsigned d)->void {
        const auto side=std::uint64_t(1)<<(32-d);
        bool inside=true;
        for(unsigned a=0;a<D;++a){
            const std::uint64_t low=decode(code,a), high=low+side-1;
            if(high<lo[a]||low>hi[a])return;
            inside &= lo[a]<=low&&high<=hi[a];
        }
        if(d<=lut_depth){
            const unsigned shift=D*(lut_depth-d);
            const auto bucket=d?std::size_t(code>>(D*(32-d)))<<shift:0;
            const auto count=lut[bucket+(std::size_t(1)<<shift)]-lut[bucket];
            if(!count)return;
            if(!inside&&d<target&&d<lut_depth&&count>512){
                for(unsigned child=0;child<(1u<<D);++child) self(self,code|(Key(child)<<(D*(31-d))),d+1);
                return;
            }
        }
        const auto last=code|suffix(d);
        const auto first=cursor<size&&keys[cursor]<code?lower(code):cursor;
        if(first==size||keys[first]>last){cursor=first;return;}
        const auto end=last==suffix(0)?size:lower(last+1);
        report(first,end,inside);cursor=end;
    };
    for(unsigned i=0;i<cells.size();++i)if(!i||cells[i]!=cells[i-1])visit(visit,cells[i],depth);
}
} // namespace morton_box
