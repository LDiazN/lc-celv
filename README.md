# CELV

## Instalación

Para instalar este proyecto, hacer `cd` a la raíz del directorio y usar el comando `make` para generar el ejecutable `celv` , y ejecutarlo para iniciar el interpretador. El comando `ayuda` presenta una lista con los comandos disponibles, y cada uno de ellos se ejecuta usando la siguiente sintaxis:

```python
<comamando> [lista de argumentos separados por espacio]

Ejemplos:

crear_dir my_dir
crear_archivo txt
escribir txt 42
leer txt
ir my_dir
celv_iniciar
```

## Implementación: Estructuras de datos

El árbol del sistema de archivos se implementa usando la estructura de persistencia generalizada que vimos en clase para estructuras de datos con forma de árbol usando cajas de cambio. A continuación, consideraremos qué datos necesita un **nodo** del árbol:

- Nombre de archivo
- Contenido en caso de ser un **documento**
- Lista de hijos en caso de ser un **************************directorio**************************
- Una referencia a tu padre, para poder volver hacia arriba
    - También podría mantenerse una pila de directorios que corresponda al path actual como una variable global, pero hacer esto complicaría la operación de actualización, como veremos más adelante.

Además, el sistema de archivos necesita mantener algunos **datos globales,** tales como:

- Version actual, en caso de haber un control de versiones (que a partir de ahora llamaremos ********celv********) activo
- Historial de **versiones** para acceder fácilmente a otras versiones
- Historial de **operaciones** para saber qué acciones se han tomado hasta ahora
    - Crear archivo
    - Editar documento
    - Eliminar archivo
    - Fusionar versiones
    - importar archivos reales
- **Directorio actual**, donde se efectuan todas las operaciones

Para representar esta información, se definieron tres estructuras de datos:

- ************************FileSystem:************************ Es una estructura que implementa las operaciones CRUD principales del programa que involucran creación y manipulación de archivos, cambio de estado del sistema de archivos, y operaciones CELV. Mantiene el ************************************directorio actual************************************ y ************************************************************la raíz del sistema de archivos************************************************************
- ************************FileTree:************************ Es una estructura **recursiva** que representa un nodo del árbol, y como tal tiene los datos necesarios para el control de versiones:
    - Tiene un apuntador a otro ******************FileTree****************** que usa como **caja de modificaciones** (la caja de modificaciones de este otro objeto se ignora). Se ignora la caja de modificaciones si no hay un celv activo. para este nodo
    - La **versión** de este nodo, es ignorada cuando no se usa control de versiones
    - un apuntador a un **********celv********** (una estructura de datos que se explica más adelante) que corresponde al manejador de versiones asignado a este archivo. Está vacío cuando no se ha inicializado un manejador de archivos que corresponda a este archivo.
    - un ****************************id de archivo**************************** que es un índice en un vector estático que contiene la información de todos los  archivos, como su nombre y contenido. Esto se hace debido a que la copia de nodo que se genera durante la actualización podría requerir copiar muchos string que podrían ser potencialmente grandes. De esta forma se reduce la necesidad de copias de archivos al mínimo, y de todas formas muchos de estos archivos nunca se eliminan realmente dado que son necesarios para versiones anteriores. Como desventaja, cuando se elimina un archivo de un árbol que no es persistente, su información sigue ahí de forma innecesaria.
    - Un **************************************Apuntador al padre************************************** de este nodo, el nodo raíz del sistema de archivo tiene este campo vacío. Este apuntador es el que nos permite ascender en el sistema de archivos
    - Un ************************************conjunto de hijos************************************ que corresponde a otros archivos en el caso de ser un directorio, este campo se ignora para documentos.
- ************CELV:************ Es una estructura de datos que administra el control de versiones del sistema de archivos correspondiente a un subarbol. Existen tantas instancias de este objeto como controles de versiones activos a lo largo del arbol, reimplementa todas las operaciones de sistema de archivos, pero haciendo uso de los atributos de control de versiones de los nodos, y añadiendo otros datos de control globales para este control de versiones:
    - tiene un ******************************vector de archivos****************************** identico al arbol de archivos normal, que contiene todas las copias que sean necesarias para mantener consistente el sistema de archivos persistente. Ahora el subarbol contenido por este este objeto será traducido de tal manera que sus id de archivo se correspondan a entradas en este vector.
    - un apuntador al ********************************************directorio de trabajo******************************************** que corresponde al directorio sobre el que se realizan las operaciones. Este apuntador es necesario para mantener la versión correcta del directorio de trabajo luego de varias operaciones de edición.
    - un arreglo de ********************versiones******************** que contiene la raíz del sistema de archivos correspondiente a cada versión creada hasta ahora. Es decir, `versiones[i]` corresponde a la raíz de la versión `i`. Este arreglo es necesario porque el árbol puede tener una cantidad de raíces proporcional al número de versiones-
    - La ********************************version actual,******************************** como un número
    - la **************************************siguiente versión disponible,************************************** como un contador
    - el **********************historial,********************** que corresponde a la lista de comandos que han sido ejecutados hasta ahora
    - un ******************************apuntador al FileTree****************************** donde fue inicializado el control de versiones. Es necesario para saber cómo volver al sistema de archivos estándar desde el persistente.

La idea con estas tres estructuras es la siguiente: 

1. **********************FileSystem********************** es la API principal que usa el usuario y modela el sistema de archivos actualmente en uso, con sus directorio de trabajo y la raíz del sistema de archivos. Las llamadas a operaciones del sistema de archivos son redirigidas al ******************FileTree****************** que corresponde al directorio de trabajo.
2. Las operaciones ******************************************************************que no usan control de versiones****************************************************************** son aquellas que se realizan sobre un `FileTree` que no tiene `CELV` . Estas operaciones son triviales y se realizan de manera ****in place**** sobre los datos del `FileTree` donde se llamen. Por otro lado, cuando el `FileTree` sí tiene un `CELV` , las operaciones **se delegan** a este, usando el `FileTree` afectado como argumento.
3. El objeto `CELV` provee la misma api de operaciones principales que  `FileTree` , pero se implementan de forma que permitan control de versiones con el estado interno de `CELV`

## Implementación: Operaciones

Las operaciones que buscamos implementar son las siguientes:

1. **************************Crear archivo**************************
2. ********************************Eliminar archivo********************************
3. ****************Escribir en documento****************
4. ********************************Cambiar de versiones********************************
5. ******************************************Cambiar de directorio******************************************
6. ******************************************Cambiar de directorio a padre******************************************
7. ******fusionar******
8. ****************importar****************
9. **************************************Inicializar el celv**************************************
10. **********************Mostrar el historial de comandos y las versiones que afectaron**********************

A continuación se explica cada operación en el control de versiones, la versión sin control de versiones se omite por trivialidad.

### Crear archivo

Para crear un archivo, se usa el directorio de trabajo actual en `CELV`, que coincide con el directorio de trabajo global. Primero, se busca linealmente si el archivo que se desea crear ya existe, y si lo hace se retorna un error, y de lo contrario, el proceso continua. Se genera una ********************************************************copia del conjunto de hijos******************************************************** del directorio actual, se crea un nodo `FileTree` para el nuevo archivo con la versión nueva, y  este se añade como  nuevo elemento a la copia. Luego la se crea un nuevo `FileTree` que corresponde a la nueva versión del nodo actual.

- Si la caja de modificaciones está vacía,  se llena con el nuevo nodo actualizado.
- Si no está vacía, se genera un evento de ******duplicación.****** Este evento indica que es necesario actualizar el padre del nodo duplicado para indicar su nueva versión.
    - Este proceso podría repetirse recursivamente hasta alcanzar la raíz del árbol, en cuyo caso, se anota la nueva raíz como raíz de la nueva versión que se creó
    - Se actuliza el directorio de trabajo de `CELV` para que la copia duplicada sea el nuevo directorio de trabajo actual.
- Se actualiza la nueva raíz de la versión. Si no se generó una nueva raíz, se usa la misma para la versión anterior. Si se generó una nueva, se usa esa como nueva raíz.

Cabe destacar que no hemos mencionado la actualización de los hijos del nodo duplicado, todos ellos deberían apuntar al nuevo nodo tras la duplicación. Esto es a propósito, puesto que en nuestras implementaciones previas notamos que actualizar los padres de estos nodos generaba mucha duplicación, volviendo nuestra optimización de árbol de versiones un poco inútil. Por esta razón se decidió ignorar este cambio, a cambio de un poco de tiempo extra para cambiar de directorio hacia arriba. Más sobre esto más adelante

**************Tiempo:**************

```python
O(MaxArchivosEnDir + AlturaArbol + log(MaxArchivosEnDir))

- MaxArchivosEnDir corresponde a la maxima cantidad de archivos en un directorio
- AlturaArbol corresponde a, bueno, la altura del arbol
- Se itera sobre los archivos en el directorio, esto se podria mejorar con búsqueda logarítmica
- Potencialmente se actualizan todos los nodos hasta la raíz
- En la práctica, este tiempo rara vez ocurre
- El tiempo de insertar el nuevo elemento en el conjunto es O(log(MaxArchivosEnDir))
```

**************Espacio**************

```python
O(1)

- Esta operación no ocupa espacio adicional, más allá del necesario para almacenar 
nuevos nodos. Como estamos usando cajas de modificación, no se copia la estructura 
completa por cada cambio
```

### Eliminar archivo

Esta operación es la inversa de la anterior, en lugar de añadir un archivo, se quita. Se genera un nuevo nodo con un elemento menos como hijo, y el resto es análogo.

 

```python
O(MaxArchivosEnDir + AlturaArbol + log(MaxArchivosEnDir))

- Misma explicación anterior
```

**************Espacio**************

```python
O(1)

- Misma explicación interior
```

### Escribir archivo

Esta operación también es muy parecida a la anterior, se busca linealmente en el directorio de trabajo por un archivo con este nombre, luego se crea un archivo con el mismo nombre en la lista de archivos de `CELV` y se asigna el nuevo contenido. Luego se genera un nuevo nodo con este nuevo id de archivo y se inserta al arbol de la misma forma que las anteriores operaciones.

### Eliminar archivo

Esta operación es la inversa de la anterior, en lugar de añadir un archivo, se quita. Se genera un nuevo nodo con un elemento menos como hijo, y el resto es análogo.

 

```python
O(MaxArchivosEnDir + AlturaArbol )

- Misma explicación anterior
- Ya no se añade el tiempo de inserción, porque no se cambia el conjunto de 
hijos de un directorio
```

**************Espacio**************

```python
O(1)

- Misma explicación interior
```

### Cambiar de versiones

Para cambiar de versiones, se deben actualizar correctamente dos datos, la versión actual y el directorio de trabajo. La primera es directa, corresponde a una asignación sencilla, pero la segunda no es tan directa. El directorio de trabajo actual puede no existir en versiones previas, y si esto pasa se desea que esté en el directorio ancestro más cercano. Para esto se implementó el siguiente algoritmo:

- empilar, desde el directorio actual hasta la raíz (excluyéndola), los **nombres** de los directorios. Esta pila corresponde al path desed la raíz al directorio de trabajo.
- Cambiar la versión actual a la versión deseada
- Intentar cambiar de directorio siguiendo el orden de la pila tanto como sea posible
    - Esta operación seguirá las reglas de la caja de cambio y las versiones
    - En el mejor de los casos, la pila se vacía y terminamos en el mismo directorio en una versión anterior
    - En el peor de los casos, acabamos en un directorio intermedio, que será el más cercano al que podamos llegar desde la pila

************Tiempo************

```python
O(AlturaArbol)

- Se itera dos veces sobre el path que lleva hasta el directorio de trabajo original
```

**************Espacio**************

```python
O(AlturaArbol)

- Se almacena una pila con el path, que tiene el mismo largo que la altura del arbol
```

### Cambiar de directorio

Cambiar de directorio hacia un directorio inferior es inmediato. Se busca linealmente en el directorio de trabajo para determinar si existe el directorio deseado. Si existe, simplemente se cambia el directorio de trabajo para apuntar a este. De lo contrario, se retorna un error.

************Tiempo************

```python
O(MaxArchivosEnDir)

- Se busca linealmente el directorio especificado
- Podria optimizarse con un contenedor que permita búsqueda logarítmica

```

**************Espacio**************

```python
O(1)

- No se usa memoria adicional
```

### Cambiar a directorio padre

Como se mencionó anteriormente, los apuntadores a padres pueden estar desactualizados. Por esa razón, en lugar de seguir el apuntador, se usa un pequeño truco. En la sección sobre ******************************************cambio de versiones,****************************************** se indicó que se empila el camino desde el directorio actual, se cambia de version, y luego se desempila tanto del camino como sea posible. Para cambiar al directorio padre correcto, usamos cambiamos a la ******************************versión actual,****************************** pero en lugar de vaciar la pila en el segundo paso, permitimos que quede exactamente un elemento en la pila, de forma que esto nos deja en el directorio padre en la versión correcta.

************Tiempo************

```python
O(AlturaArbol)

- Igual que en cambio de versiones

```

**************Espacio**************

```python
O(AlturaArbol)

- Igual que en cambio de versiones
```

### Fusionar

La operación no se implemento. 

Sin embargo a continuación se describe la lógica que tendría una implementación deseada.

Al fusionar versiones se puede reciclar alguna de las raíces de las versiones como raíz nueva (pues a fin de cuentas la estructura soporta cambios y persistencia).

Para esto se escoge la raíz de alguna de las 2 versiones como raíz de la version de fusión.

La fusión actuaría como una unión sobre el contenido de directorios comunes a ambas versiones. Es decir, aquellos archivos comunes a ambas versiones que además  tengan la misma versión se dejan intactos, el resto se incluye según sea necesario.

Como se van a comparar archivos en directorios en paralelo sobre árboles de versión distintos, conviene utilizar un recorrido como BFS, que por ser en amplitud facilita el unir los archivos por nivel a la vez que se actualiza el árbol nuevo de versiones de forma ordenada.

Para que esto funcione, la cola del BFS debe guardar referencias a los directorios comunes a ambas versiones, pero cuyas versiones difieren (digamos $src$ y $dst$). Tambien debe guardar una referencia al mismo directorio de la versión actual (posiblemente nuevo, digamos $m\_ver$).

Se guardan estas referencias a directorios porque corresponden a los directorios en donde hay diferencias entre versiones y el último porque corresponde a la versión destino.

Cada vez que se desempile, se actualiza el directorio actual (usando la ultima referencia, $m\_ver$).

Luego se revisa el contenido de los directorios asociados a las referencias de las versiones de origen y destino. Esta revisión debe ser en paralelo, pues se está intentando unir un directorio común , pero con diferencias entre las versiones origen y destino. 

Para hacer esta comparación en paralelo sirve ***ordenar*** el contenido (adyacentes) de cada directorio por nombre, e implementar la unión de los directorios con una lógica similar a la de `merge` en `mergesort`:

<aside>
💡 Ejemplo en `haskell` del código de `merge` del que se habla

```haskell
merge :: Ord a => [a] -> [a] -> [a]
merge xs [] = xs
merge [] xs = xs
merge (x:xs) (y:ys)
	| x < y = x : merge xs (y:ys) -- 1
	| y < x = y : merge (x:xs) ys -- 2
	| otherwise = x : merge xs ys -- 3
```

Notese que el codigo de arriba actúa como una unión **si las listas de entrada están ordenadas**.

</aside>

- Siempre que ambos directorios aún tengan archivos por ver, se revisan en base a la **lógica de fusión** . En este caso se toma en cuenta que ************************************************************************si directorios en versiones distintas tienen el mismo nombre, pero una versión distinta asociada, se empilan************************************************************************ (corresponde al caso 3 del ejemplo)
- Si sólo uno de los directorios tiene archivos por ver, entonces los archivos  no eran comunes a ambas versiones y se incluyen en la versión de fusión. En este caso no se empila nada, solo se añaden archivos. Corresponde a los casos 1 y 2 según sea necesario)

La logica de fusión está encapsulada por lo siguiente. Dados 2 archivos cualesquiera pertenecientes a directorios comunes de ambas versiones y sean $u$ y $v$ dichos directorios:

- Si la siguiente entrada de $u$ y $v$ representa un archivo del mismo nombre, entonces
    - Si la siguiente entrada de $u$ y $v$ tiene el mismo tipo (`DIRECTORY` o  `REGULAR`), entonces
        - Si la siguiente entrada de $u$  y $v$ tienen la misma version, entonces
            - ****NO**** se hace copia, no hace falta.
        - De lo contrario
            - Si son directorios se empilan ambos.
            - Si son archivo se aplica **diff**.
    - De lo contrario
        - Se añade primero **********************************************************la entrada que sea un archivo regular********************************************************** y se considera la siguiente entrada de su directorio asociado (digamos $u$)
- De lo contrario
    - Se añade la entrada que compare como menor (lexicograficamente) y se considera la siguiente entrada de su directorio asociado (digamos $u$)

> Se añade primero la entrada de un **archivo regular** y después de directorio pues así se escoge ordenar a los adyacentes y la consistencia es necesaria para que la unión sea correcta. Poner los directorios primeros también sería una opción, de haber ordenado así las entradas para los adyacentes en un principio.
> 

El `diff` que se llama entre archivos regulares de versiones distintas no es más que una modificación del problema **********EDIST********** que recupera un string en donde se reporta el mínimo numero de cambios en el contenido del archivo entre versiones.

### Importar

Se empleó el header `filesystem` para recorrer el árbol de directorios local.

Primero se busca linealmente sobre los hijos del directorio de trabajo para buscar si ya existe un directorio con el mismo nombre. Si existe, se retorna un error, de lo contrario, el proceso continua.

Luego, se crea un `FileTree` que imita el directorio local de la siguiente forma:

1. Se crea un `FileTree` sin hijos que funciona de raíz.
2. Se itera sobre los hijos del directorio especificado, con ayuda de un `std::filesyste::directory_iterator` .
3. Si el archivo actual no tiene permisos para ser leido o es un enlace, se levanta una advertencia y se ignora
4. Si el archivo es un documento de texto, se crea un nuevo `FileTree` con su contenido como y nombre, y se agrega como hijo del directorio actual
5. Si el archivo es directorio, se hace una llamada recursiva para clonar este subarbol de archivos, y el subarbol generado de asigna como hijo de la raíz actual
6. A ************todos************ los nodos generados por este proceso se les configura correctamente su `CELV` en caso de haber uno en existencia.

Finalmente, este nuevo `FileTree` se añade como hijo del directorio de trabajo, siguiendo la misma lógica para crear archivo que se explicó anteriormente.

************Tiempo************

```python
O(CantidadArchivosLocales + MaxArchivosEnDir + AlturaArbol)
- La busqueda lineal del archivo al inicio del proceso toma tiempo O(MaxArchivosEnDir)
- El recorrido del arbol de archivos local para realizar la copia toma O(CantidadArchivosLocales)
- La inserción de un elemento del arbol toma tiempo O(AlturaArbol)
```

**************Espacio**************

```python
O(AlturaArbolLocal)
- Aunque se crea todo un arbol nuevo, este no es espacio extra, corresponde al espacio
requerido por el resultado que queremos
- El espacio necesario para el proceso es el que toma la pila de la recursión, 
dado que se usa un recorrido en profundidad. Como la máxima cantidad de archivos que
pueden empilarse es igual a la altura del subarbol de archivos local, entonces este 
es el espacio requerido.
```

### Inicializar CELV

Para inicializar el `CELV`, primero se visita todo el subarbol buscando un nodo que ya tenga un `CELV` inicializado. Si no existe, se crea un nuevo `CELV` y se asigna al nodo correspondiente al directorio de trabajo actual. Luego, se genera un clon de todo el subarbol que empieza con el directorio de trabajo y se almacena en `CELV`, de ahora en adelante todos los cambios en este subarbol son administrados por `CELV`.  Además, todos los nodos del subarbol son actualizados con un apuntador al `CELV` creado, y sus id de archivo son actualizados para que apunten a una lista de archivos administrada por `CELV`. De esta forma  se pueden duplicar archivos cuando se hacen operaciones de escritura, y cuando se elimina el control de versiones, estos archivos duplicados tambien se liberan.

************Tiempo************

```python
O(NArchivos)

- Se recorre el subarbol una cantidad finita de veces, que en el peor de los 
casos es todo el arbol.

```

**************Espacio**************

```python
O(NArchivos)

- Se genera una copia de un subarbol, asi que potencialmente se usa un espacio 
proporcional a todo el arbol 
```

Esta es una de las operaciones más costosas, pero al menos es de las que se hacen menos seguido, una inicialización.
