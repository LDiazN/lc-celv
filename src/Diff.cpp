#include<stdio.h>
#include<iostream> 
#include<vector>
#include<queue> 
#include<algorithm> 

using namespace std; 

// TODO fix error in produce_diff
// TODO check empty string
// TODO throw on invalid enum case
// TODO mejorar appends
// TODO add hpp
// TODO  remove std
// TODO arrange visibility

//GC

enum STATE { INSERT, DELETE, MODIFY, NOTHING };

struct CELL { 
    int i, j, best; 
    STATE state; 
};

class DIFF {
public:
    vector<vector<CELL>> memo;
    string A, B; 

    DIFF(const string &u, const string &v) : A(u), B(v), memo(u.size()) {
        // Initialize table size
        for (int i=0; i<memo.size(); ++i)
            memo[i].resize(B.size());
    }

    void diff() {
        // Precalcula la edist para recuperar camino
        edist();
        string TMP;

        // recupera el camino
        produce_diff(TMP);
        cout<<TMP<<'\n';
    }

    int edist() {
        // Set edist for base cases
        for (int i=0; i<A.size(); ++i)
            memo[i][0] = CELL{0, 0, i, DELETE};
        for (int i=1; i<B.size(); ++i)
            memo[0][i] = CELL{0, 0, i, INSERT};

        for (int i=1; i<A.size(); ++i)
            for (int j=1; j<B.size(); ++j)
                if (A[i] == B[j])
                    memo[i][j] = CELL{i - 1,j - 1, memo[i-1][j-1].best, NOTHING};
                else 
                {
                    if (memo[i][j-1].best < memo[i-1][j].best) 
                    {
                        if (memo[i][j-1].best < memo[i-1][j-1].best)  // ! ADDING TO A
                            memo[i][j] = CELL{i, j-1, 1 + memo[i][j-1].best, INSERT};
                        else  // ! MODIFYING
                            memo[i][j] = CELL{i-1, j-1,1 + memo[i-1][j-1].best, MODIFY};
                    }
                    else 
                    {
                        if (memo[i-1][j].best < memo[i-1][j-1].best)  // ! DELETING FROM A
                            memo[i][j] = CELL{i-1, j, 1 + memo[i-1][j].best, DELETE};
                        else  // ! MODIFYING
                            memo[i][j] = CELL{i-1, j-1, 1 + memo[i-1][j-1].best, MODIFY};
                    }
                }

        return memo[A.size()-1][B.size()-1].best;
    }

    void DBG() {
        cout<<"Edist between "<<A<<" and "<<B<<" is: "<<edist()<<'\n';

        for (auto row : memo)
        {
            for (auto cell : row)
                printf(" %d %d (%d) %d |", cell.i, cell.j, cell.best, cell.state);
            printf("\n");
        }

        navigate_ancestors(A.size()-1, B.size()-1); printf("\n");
    }

    void produce_diff(string &result_string) {
        int u = A.size() - 1, v = B.size() - 1;
        STATE current_state = memo[u][v].state;

        while (memo[u][v].i != u || memo[u][v].j != v ) // while not in root
        { 

            switch (current_state) {
                case INSERT:
                {
                    bool tmp = true;
                    // ! suffix
                    result_string.append("}}");
                    while (memo[u][v].state == current_state) // ! while state is the same
                    {
                        result_string.push_back(B[v]); 
                        u = memo[u][v].i; v = memo[u][v].j;

                        if (memo[u][v].i == u && memo[u][v].j == v)
                            if (tmp)
                                tmp = false;
                            else
                                break;
                    } 
                    // ! preffix
                    result_string.append("{{");
                    break;
                }
                case DELETE:
                {
                    bool tmp = true;
                    result_string.append("]]");
                    while (memo[u][v].state == current_state )
                    {
                        result_string.push_back(A[u]); 
                        u = memo[u][v].i; v = memo[u][v].j;

                        if (memo[u][v].i == u && memo[u][v].j == v)
                            if (tmp)
                                tmp = false;
                            else 
                                break;
                    }
                    result_string.append("[[");
                    break;
                }
                case MODIFY:
                {
                    bool tmp = true;
                    result_string.append("}}");
                    string TEMP("]]");
                    while (memo[u][v].state == current_state  )
                    {
                        result_string.push_back(B[v]); TEMP.push_back(A[u]); 
                        u = memo[u][v].i; v = memo[u][v].j;
                        if (memo[u][v].i == u && memo[u][v].j == v)
                            if (tmp)
                                tmp = false;
                            else
                                break;
                    }
                    TEMP.append("[[");
                    result_string.append("{{");
                    result_string.append(TEMP);
                    break;
                }
                case NOTHING:
                {
                    bool tmp = true;
                    while (memo[u][v].state == current_state) 
                    {
                        result_string.push_back(A[u]);
                        u = memo[u][v].i; v = memo[u][v].j;
                        if (memo[u][v].i == u && memo[u][v].j == v)
                            if (tmp)
                                tmp = false;
                            else
                                break;
                    }
                    break;
                }
                default:
                    printf("ERROR!"); // !OJO throw exept
                    break;
            }

            current_state = memo[u][v].state;

        }

        reverse(result_string.begin(), result_string.end());
    }

    void navigate_ancestors(int u, int v) {
        if (memo[u][v].i != u || memo[u][v].j != v) 
        {
            navigate_ancestors(memo[u][v].i, memo[u][v].j);
            switch (memo[u][v].state) {
                case INSERT:
                    printf("INSERT ");
                    break;
                case DELETE:
                    printf("DELETE ");
                    break;
                case MODIFY:
                    printf("MODIFY ");
                    break;
                case NOTHING:
                    printf("NOTHING ");
                    break;
            }
        }
    }

};

int main() { 
    string A,B;
    getline(cin, A);
    getline(cin, B);
    struct DIFF solve(A,B);
    solve.diff();
}
