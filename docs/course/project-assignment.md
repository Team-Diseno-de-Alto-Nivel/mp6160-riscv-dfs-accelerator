**Instituto Tecnológico de Costa Rica** 

**MP6160 Diseño de Alto Nivel de Sistemas Electrónicos: II Cuatrimestre 2026 Profesor: Luis G. León-Vega, Ph.D** 

**Evaluación: Proyecto** 

## **1. Objetivo** 

El proyecto del curso de Diseño de Alto Nivel de Sistemas Electrónicos busca ejercitar los conocimientos adquiridos a través de los contenidos vistos en clase, sumado a una investigación científica que contribuya puntualmente a la mejora del estado del arte. 

## **2. Aspectos Logísitos** 

El proyecto es de ejecución grupal, con **grupos de 4-5 personas** para facilitar la distribución de las tareas de investigación y experimentación. Para efectos experimentales donde aplique y se necesite, el profesor proveerá acceso a la plataforma con una FPGA, sea una AMD Kria KV260 proporcionada por el ITCR en San Carlos o una AMD Alveo U55C por AMD/ETH Zürich. 

El avance del proyecto se cuantificará mediante cuatro avances puntuales: 

- Estudio preliminar del estado del arte (Semana 5): propuesta del tema de investigación y revisión de soluciones existentes. 

- Avance I (Semana 6): propuesta del diseño de II nivel (arquitectura + modelo de caja gris/blanca). En este avance, se espera describir los módulos a nivel funcional mediante un diagrama de bloques, sin llegar a implementarlos ni describirlos a alto nivel. Asimismo, se debe especificar la herramienta que se utilizará para el modelado de III nivel en alto nivel. 

- Avance II (Semana 9): avance del diseño descrito a alto nivel. Debe ser mediante un video de 1 minuto y con link al repositorio (público). 

- Defensa y Entrega del Artículo (Semana 11): presentación de resultados y contraste contra soluciones existentes. 

Dado el carácter científico de la maestría, se recomienda que los temas sean afines a la tesis de los integrantes a la medida de lo posible. Asimismo, los artículos deben tener calidad de producción científica, con seis páginas IEEE. Todo el desarrollo a nivel de implementación del modelo debe ser versionado usando Git, tomando en cuenta aspectos como reproducibilidad, montaje del entorno y resultados esperados. 

## **3. Estudio preliminar del estado del arte** 

**Entrega** : Semana 5 a través del TEC Digital 

Para este avance se contempla la definición del tema de investigación del grupo de trabajo de forma general sin entrar en detalles de implementación (o microarquitectura). Este tema será puesto como el título provisional del artículo científico que se entregará al final del curso. Algunos ejemplos de temas son: 

- _Desarrollo de un procesador RISC-V con ejecución fuera de orden y coprocesamiento vectorial_ : en este caso, el procesador debe estar equipado con las extensiones RV32IMFC y RVV. Debe ser capaz de ejecutar software en bare metal. 

- _Desarrollo de una GPU basada en RISC-V_ : la idea es utilizar instrucciones transpiladas de PTX/ROCm en RISC-V. A nivel de software, debe poderse programar con el paradigma SIMT, donde cada thread puede corresponder a un núcleo mínimo de RISC-V y debe ser capaz de mapear correctamente los bloques/grids. A nivel de hardware, cada unidad computacional puede ser escalar o vectorial, según su diseño. Como requisito esencial, debe estar basado en RISC-V y tener soporte de enteros y punto flotante. 

- _Desarrollo de una celda reconfigurable de CGRA_ : en este caso, se busca modelar una CGRA heterogénea que posea, tentativamente: 1) celdas de ruteo de datos (con FIFOs), 2) celdas con bancos de memoria distribuida, 3) celdas de procesamiento escalar, 4) celdas de procesamiento vectorial. En este caso, para la intercomunicación, puede considerarse una comunicación de tipo NoC y procesamiento capaz de ejecutar lógica de enteros de 8, 16 y 32 bits, así como punto flotante de 32 bits. Debe ser capaz de ejecutar programas codificadas en RISC-V. 

Un tema inadecuado en este contexto es uno muy general (que no acote la contribución como tal a nivel de arquitectura) o uno muy específico, que especifique la técnica a utilizar como tal, ya que esto último limitaría posibles giros en la investigación cuando se haga el estudio del estado del arte (SOTA). A mitad de la investigación (final de semana 4, inicios de la semana 5, aproximadamente), se le solicita a cada grupo agendar una reunión con el profesor para recibir retroalimentación. 

Posterior a la elección del tema, se sugiere realizar una investigación del estado del arte alrededor del tema de interés, usando fuentes confiables e indexadas con no más de 10 años de antigüedad. Se sugiere el uso de IEEE Xplore, ACM Digital Library, Connected Papers (o similares). El enfoque de esta búsqueda debe dirigirse a buscar al menos dos soluciones similares con resultados reproducibles y otras cinco cuyas soluciones cubren parcialmente el problema. 

Luego de haber encontrado las referencias, debe sintetizar la _Introducción_ (1 página máximo) y el _Background and Related Work_ del artículo científico en calidad de producción (1 página máximo). Por lo tanto, el entregable es el PDF del artículo con el título preliminar, los autores, la introducción y el _background and related work_ . 

## **4. Avance 1** 

**Entrega** : Semana 6 a través del TEC Digital 

Para este avance se contempla la propuesta de arquitectura (de II nivel) a realizar para solucionar el problema planteado. En este caso, se debe explicar cómo la solución al problema es novedosa en comparación con otras soluciones existentes. Para ello, tome en cuenta los siguientes criterios: 

- _Estudio de otras soluciones_ : destaque las fortalezas y debilidades de las otras soluciones presentes en el SOTA, destacando en qué situaciones no son viables y en cuáles sí. Se debe culminar con una lista de desafíos observados. 

- _Propuesta de solución_ : se debe presentar la solución, justificando cómo resuelve cada desafío, su alcance y explicando en qué consiste. Ayúdese de diagramas de nivel. 

- _Plan de experimento_ : se debe proponer una lista de experimentos que se realizarán para contrastar las soluciones existentes (por ejemplo: distintos tamaños de datos, distintos dominios de datos, etc) y cuáles métricas se planean tomar para el contraste posterior (por ejemplo, consumo de recursos, latencia, timing y OP/s). También se incluye las herramientas a utilizar, con sus respectivas versiones. 

Luego de haber resuelto los criterios anteriores, debe sintetizar la _Propuesta de Solución_ (1 página máximo) y el _Results and Analysis_ (1 página máximo). Por lo tanto, el entregable es el PDF del artículo hecho en el estudio del estado del arte con esta nueva integración en el mismo documento. 

## **5. Avance 2** 

**Entrega:** Semana 9 a través del TEC Digital 

Para este avance se contempla la demostración funcional de la solución propuesta en el Avance I. El objetivo es evidenciar que la arquitectura planteada puede implementarse y evaluarse mediante . 

Para ello, tome en cuenta los siguientes criterios: 

- _Implementación de la solución:_ se debe presentar la implementación de la arquitectura propuesta, ya sea parcial (mínimo un 80 %) o completa, incluyendo los módulos principales del diseño en un repositorio público en GitHub. Se debe describir brevemente cómo se integran los componentes y las herramientas utilizadas (por ejemplo: HDL, HLS, SystemC, LiteX, IPs o frameworks específicos), tanto en un README como en el documento, completando el apartado de Propuesta de solución. 

- _Demostración experimental:_ se debe demostrar el funcionamiento de la solución mediante pruebas reproducibles. Estas pruebas deben ejecutarse en simulación o emulación, mostrando que el sistema cumple con el comportamiento esperado. Esto será mediante un video de no más de un minuto. 

- _Resultados preliminares:_ se deben presentar resultados iniciales de los experimentos planteados en el Avance I. Estas pueden incluir métricas como la utilización de recursos, la latencia, la frecuencia máxima alcanzada, el throughput u otras métricas relevantes según el problema estudiado. 

Luego de haber resuelto los criterios anteriores, debe mejorar la **Propuesta de Solución** (1.5 páginas máximo) y una versión preliminar del **Results and Analysis** (1.5 páginas máximo) con el plan de experimentos y los resultados. Por lo tanto, el entregable es un archivo comprimido en el TEC Digital con el PDF del artículo actualizado con estas nuevas secciones integradas en el mismo documento, una citación al repositorio en el apartado de la **Propuesta de Solución** y el video. 

## **6. Defensa** 

**Entrega:** Semana 11 (según cronograma del curso) 

La defensa consiste en la presentación final del proyecto de investigación realizado durante el curso. En esta etapa cada grupo deberá exponer la solución desarrollada, los resultados obtenidos y el contraste con respecto a las soluciones existentes en el estado del arte. 

Para ello, tome en cuenta los siguientes criterios: 

_Presentación de la solución:_ se debe presentar la arquitectura final desarrollada, explicando su funcionamiento, los principales componentes del diseño y las decisiones de diseño tomadas durante el desarrollo del proyecto. 

   - Se presenta la arquitectura final y sus componentes (5 pts). 

   - Se justifican las decisiones de diseño (5 pts). 

   - Hay claridad y seguimiento adecuado del flujo de diseño (5 pts). 

   - Se justifica la pertinencia de la solución (5 pts). 

- _Resultados experimentales:_ se deben presentar los resultados obtenidos a partir de los experimentos planteados en el Avance I, mostrando métricas relevantes como utilización de recursos, latencia, frecuencia alcanzada, throughput u otras métricas pertinentes al problema estudiado. 

   - Se presentan y se explican las métricas que se utilizarán (5 pts). 

   - Se explica el experimento que permite sacar resultados (5 pts). 

- _Contraste con el estado del arte:_ se debe comparar la solución desarrollada con otras soluciones presentes en el estado del arte, destacando fortalezas, limitaciones y posibles escenarios donde la propuesta resulte más adecuada. 

   - Se mencionan y explican brevemente las alternativas del estado del arte (5 pts). 

   - Se contrastan con el diseño realizado de forma cualitativa (5 pts). 

   - Se contrastan con el diseño de forma cuantitativa (5 pts). 

- _Discusión y conclusiones:_ se deben discutir los principales hallazgos del trabajo, las limitaciones observadas y las posibles líneas de trabajo futuras. 

   - Se identifica la contribución y se justifica (5 pts). 

   - Se identifican limitaciones en la generalización del diseño (5 pts). 

   - Se identifica trabajo futuro (5 pts). 

La defensa tendrá una duración aproximada de 15 minutos por grupo, seguida de una sesión de preguntas. El material de apoyo puede consistir en diapositivas, demostraciones del sistema o resultados experimentales relevantes. 

Cumplimiento del tema propuesto (20 pts). 

- Evaluación general del desempeño del grupo (20 pts). 

La tabla de rúbrica: 

Cuadro 1: Rúbrica de evaluación de la defensa fnal 

|**Criterio**|**Indicador**|**Puntaje**|**Obtenido**|
|---|---|---|---|
|**Presentación de la solución**|Se presenta la arquitectura fnal y sus com-<br>ponentes.|5 pts||
||Se justifcan las decisiones de diseño.|5 pts||
||Hay claridad y seguimiento adecuado del fu-<br>jo de diseño.|5 pts||
||Se justifca la pertinencia de la solución.|5 pts||
|**Resultados experimentales**|Se presentan y se explican las métricas que<br>se utilizarán.|5 pts||
||Se explica el experimento que permite sacar<br>resultados.|5 pts||
|**Contraste con el estado del arte**|Se mencionan y explican brevemente las al-<br>ternativas del estado del arte.|5 pts||
||Se contrastan con el diseño realizado de for-<br>ma cualitativa.|5 pts||
||Se contrastan con el diseño de forma cuanti-<br>tativa.|5 pts||
|**Discusión y conclusiones**|Se identifca la contribución y se justifca.|5 pts||
||Se identifcan limitaciones en la generaliza-<br>ción del diseño.|5 pts||
||Se identifca trabajo futuro.|5 pts||
|**Aspectos generales**|Cumplimiento del tema propuesto.|20 pts||
||Evaluación general del desempeño del gru-<br>po.|20 pts||
||**Total**|**100 pts**||



## **7. Artítulo Científico** 

El entregable final del proyecto consiste en un artículo científico que describa la investigación realizada durante el curso. Este documento debe presentar claramente el problema abordado, la solución propuesta, la metodología experimental y el análisis de los resultados obtenidos. 

El artículo debe seguir el formato de conferencias del IEEE y tener una extensión máxima de seis páginas (excluyendo referencias). Se espera que el documento tenga calidad de producción científica, con redacción clara, figuras legibles y resultados reproducibles. 

Se recomienda que el artículo incluya, como mínimo, las siguientes secciones: 

- _Abstract:_ resumen de no más de 250 palabras. 

- _Introducción:_ presentación del problema, motivación y contribución del trabajo. 

- _Background and Related Work:_ revisión del estado del arte y comparación con trabajos relevantes. 

- _Propuesta de Solución / Arquitectura:_ descripción detallada de la arquitectura propuesta y su funcionamiento. 

- **Resultados y Análisis:** presentación de las métricas y el diseño del experimento, presentación de resultados y discusión de los resultados obtenidos. 

- **Conclusiones y Trabajo Futuro:** síntesis de los hallazgos principales y de posibles líneas de investigación futura. 

El entregable consiste en el PDF final del artículo en formato IEEE en el TEC Digital. 

