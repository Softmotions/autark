
1. Executes `settle` of all first level `check` rules
   in order to build env of all conditional rules.

2. Executes `settle` of other rules until no new unsettled rules appeared.

3. Executes `build` of project root rule.