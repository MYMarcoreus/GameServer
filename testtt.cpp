#include <bits/stdc++.h>
using namespace std;

// @lc code=start
class Solution {
public:
    vector<int> topKFrequent(vector<int>& nums, int k) {
        vector<int> Rst;
        std::unordered_map<int, int> FreqTable;
        for(auto num : nums) {
            FreqTable[num]++;
        }

        std::priority_queue<int> Q{};
        for (const auto key : FreqTable | views::keys)
        {
            Q.push(key);
        }
    }
};