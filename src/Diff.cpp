#include<stdio.h>
#include<iostream> 
#include<algorithm> 
#include<cassert>
#include<sstream>
#include<stdexcept>

#include"Diff.hpp"

DIFF::DIFF(const std::string &u, const std::string &v) 
        : _A(u)
        , _B(v)
        , _memo(u.size() + 1) 
{
    //assert( (u.size() > 0 || v.size() > 0) && "Both strings are empty!");

    // Initialize table size
    for (size_t i=0; i<_memo.size(); ++i)
        _memo[i].resize(_B.size() + 1);
}

std::string DIFF::compute_diff() 
{
    if (_A.compare(_B) != 0)
    {
        // Precalcula la edist para recuperar camino
        edist_pdist();

        // Recupera el camino
        return produce_diff();
    }
    return _A;
}

int DIFF::edist_pdist() 
{
    // Esquina superior izquierda actua como caso base 
    _memo[0][0] = CELL{0, 0, 0, NOTHING};

    // Inicializa tabla _memo con casos bases 
    for (int i=1; i<=static_cast<int>(_A.size()); ++i)
        _memo[i][0] = CELL{i-1, 0, 1 + _memo[i-1][0].best, DELETE};

    for (int i=1; i<=static_cast<int>(_B.size()); ++i)
        _memo[0][i] = CELL{0, i-1, 1 + _memo[0][i-1].best, INSERT};

    for (int i=1; i<=static_cast<int>(_A.size()); ++i)
        for (int j=1; j<=static_cast<int>(_B.size()); ++j)
            if (_A[i - 1] == _B[j - 1]) 
                _memo[i][j] = CELL{i - 1,j - 1, _memo[i - 1][j - 1].best, NOTHING};
            else 
            {
                if (_memo[i][j - 1].best < _memo[i - 1][j].best) 
                {
                    if (_memo[i][j - 1].best < _memo[i - 1][j - 1].best)  // Insertion case 
                        _memo[i][j] = CELL{i, j-1, 1 + _memo[i][j - 1].best, INSERT}; 
                    else  // Modifying case 
                        _memo[i][j] = CELL{i - 1, j - 1,1 + _memo[i - 1][j - 1].best, MODIFY};
                }
                else 
                {
                    if (_memo[i - 1][j].best < _memo[i - 1][j - 1].best) // Deletion case 
                        _memo[i][j] = CELL{i - 1, j, 1 + _memo[i - 1][j].best, DELETE};
                    else  // Modifying case
                        _memo[i][j] = CELL{i - 1, j - 1, 1 + _memo[i - 1][j - 1].best, MODIFY};
                }
            }

    return _memo[_A.size()][_B.size()].best;
}

std::string DIFF::produce_diff() 
{
    int u = _A.size() , v = _B.size() ;
    STATE current_state = _memo[u][v].state;
    std::stringstream ss;

    while (_memo[u][v].i != u || _memo[u][v].j != v) // Mientras no sea el caso base de la tabla 
    { 
        switch (current_state) {
            case INSERT:
            {
                ss << CLOSING_NEW_VER;
                while (_memo[u][v].state == current_state && (_memo[u][v].i != u || _memo[u][v].j != v)) 
                {
                    ss << _B[v-1]; 
                    u = _memo[u][v].i; v = _memo[u][v].j;
                } 
                ss << OPENING_NEW_VER;
                break;
            }
            case DELETE:
            {
                ss << CLOSING_OLD_VER;
                while (_memo[u][v].state == current_state && (_memo[u][v].i != u || _memo[u][v].j != v))
                {
                    ss << _A[u-1]; 
                    u = _memo[u][v].i; v = _memo[u][v].j;
                }
                ss << OPENING_OLD_VER;
                break;
            }
            case MODIFY:
            {
                ss << CLOSING_NEW_VER;

                std::stringstream temp_ss;
                temp_ss << CLOSING_OLD_VER;
                while (_memo[u][v].state == current_state && (_memo[u][v].i != u || _memo[u][v].j != v))
                {
                    ss << _B[v-1]; 
                    temp_ss << _A[u-1]; 
                    u = _memo[u][v].i; v = _memo[u][v].j;
                }
                temp_ss << OPENING_OLD_VER;
                ss << OPENING_NEW_VER;
                ss << temp_ss.str();
                break;
            }
            case NOTHING:
            {
                while (_memo[u][v].state == current_state &&  (_memo[u][v].i != u || _memo[u][v].j != v)) 
                {
                    ss << _A[u-1];
                    u = _memo[u][v].i; v = _memo[u][v].j;
                }
                break;
            }
            default:
                throw std::range_error("Invalid value for ennumeration.");
                break;
        }

        // Actualiza estado
        current_state = _memo[u][v].state;

    }
    
    // Dado que el std::string se produjo en orden inverso, se invierte
    std::string result(ss.str());
    reverse(result.begin(), result.end());
    return result;
}

void DIFF::DBG() 
{
    std::cout<<"Edist between "<<_A<<" and "<<_B<<" is: "<<edist_pdist()<<'\n';

    for (int i=0; i<static_cast<int>(_memo[0].size()); ++i)
        printf("          %d ", i);
    printf("\n");

    for (int i=0; i<static_cast<int>(_memo.size()); ++i)
    {
        for (int j=0; j<static_cast<int>(_memo[i].size()); ++j)
        {
            if (j == 0)
                printf("%d:", i);
            printf(" %d %d (%d) %d |", _memo[i][j].i, _memo[i][j].j, _memo[i][j].best, _memo[i][j].state);
        }
        printf("\n");
    }

    printf("Sequence of instructions: "); navigate_ancestors(_A.size()-1, _B.size()-1); printf("\n");
}

void DIFF::navigate_ancestors(int u, int v) 
{
    if (_memo[u][v].i != u || _memo[u][v].j != v) 
        navigate_ancestors(_memo[u][v].i, _memo[u][v].j);
    switch (_memo[u][v].state) {
        case INSERT:
            printf("INSERT %c -> ",_B[v]);
            break;
        case DELETE:
            printf("DELETE %c -> ",_A[u]);
            break;
        case MODIFY:
        printf("MODIFY %c => %c -> ", _A[u], _B[v]);
            break;
        case NOTHING:
            printf("NOTHING %c -> ",_A[u]);
            break;
    }

}