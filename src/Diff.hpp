#ifndef DIFF_HPP
#define DIFF_HPP

#include<string>
#include<vector>

#define OPENING_OLD_VER "[["
#define CLOSING_OLD_VER "]]"
#define OPENING_NEW_VER "{{"
#define CLOSING_NEW_VER "}}"

// Estados posibles de la tabla. Indican que se hizo para cualquier _memo[i][j]
enum STATE { INSERT, DELETE, MODIFY, NOTHING };

// Definicion de celda de la tabla _memo
struct CELL { 
    int i, j, best; 
    STATE state; 
};

/// @brief Implementacion de un `diff` para distinguir cambios minimos en cadenas de caracteres.
class DIFF {
public:
    /// @brief Inicializa las estructuras necesarias para calcular la diferencia.
    /// Incurre en error si ambas cadenas estan vacias .
    /// @param u string de origen
    /// @param v string de destino
    DIFF(const std::string &u, const std::string &v);

    /// @brief Realiza las operaciones necesarias para producir el string de diferencias minimas entre u y v.
    /// @return String que indica diferencias mínimas entre u y v.
    std::string compute_diff() ;

private:
    /// @brief A partir de edist, calcula el string de diferencias mínimas entre u y v.
    /// @return String que indica diferencias mínimas entre u y v.
    std::string produce_diff() ;

    /// @brief Algoritmo de edist extendido para recuperar los cambios minimos. 
    /// @return Distancia mínima de edición. 
    int edist_pdist() ;

    void DBG() ;
    void navigate_ancestors(int u, int v) ;

    std::string _A, _B; 
    std::vector<std::vector<CELL>> _memo;
};

#endif