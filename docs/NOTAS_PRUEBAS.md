# Notas de pruebas (versión privada)

Registro de lo probado con amigos: qué funciona, qué falla y pistas técnicas.
Direcciones = gta_sa.exe 1.0 US (HOODLUM, MD5 170b3a9108687b26da2d8901c6948a18).

## Funciona (prueba 2026-11-04, 2 jugadores por túnel playit.gg)
- Conexión por túnel playit (host + 1 amigo).
- Se ven las armas que el otro saca con trucos.
- Se ve la barra de vida actualizada (HESOYAM).
- Se ve el jetpack del otro.
- El amigo puede subirse a autos spawneados por el host.

## Errores / pendientes

### 1. Crash con el truco "recompensa por tu cabeza" (BAGOWPG)
- Crash en 0x0063A04B (write, ECX=0) dentro de 0x63A040, llamado desde 0x6715B0.
- 0x6715A6: `operator new(0x64)` (0x41B7C0) devolvió NULL -> el juego no lo comprueba.
- Hipótesis: pool de eventos/IA agotado (todos los peatones atacan a la vez; con jugadores
  de red hay más objetivos). Fix probable: ampliar límites de pools (estilo limit adjuster)
  y/o bloquear/sincronizar trucos de caos de peatones.
- Además: los trucos NO se sincronizan (TODO de CoopAndreas: "cheat code sync").

### 2. Crash al subir dos jugadores al mismo vehículo (bici)
- Crash en 0x00523E6B (read, EAX=0), log 2026-11-04_07-02-12 (PC del host).
- 0x523E5D: `ped->m_pIntelligence (+0x47C) -> taskManager (+4) -> GetActiveTask()` (0x681720)
  devuelve NULL y se desreferencia sin comprobar (compara el id de tarea con 0x2BD).
- Hipótesis: al ir de pasajero en una bici (sincronización de pasajero), el ped queda un
  frame sin tarea activa y este código (zona de cámara/ped, 0x523xxx) no lo tolera.
  Fix probable: hook en ese punto que salte el bloque si GetActiveTask() == NULL, y revisar
  CPassengerEnter para bicis/motos.

### 3. Autos de Ryder y Sweet duplicados frente a la casa de CJ
- Los generadores de autos / script los crean en cada PC -> aparecen dos copias.
- Revisar sincronización de car generators (CPatch desactiva CTheCarGenerators::Process para
  no-host; ver si el script MAIN/INITIAL crea esos autos en el cliente).

### 4. Crash con el truco "quitar nivel de busqueda" (TURNDOWNTHEHEAT)
- Lo uso el amigo (cliente) sin tener policias; quien tenia nivel de busqueda era el host.
- Crash en 0x006E2DB4 (write), log 2026-11-04_05-07-55 (PC del amigo).
- 0x6E2D90 = CVehicle::operator delete (devuelve el objeto al pool de vehiculos 0xB74494),
  llamado desde un destructor de vehiculo (0x6B8960). El indice calculado sale negativo
  (EDX=0xFFFDB78D) -> el puntero NO pertenece al pool: vehiculo ya borrado (doble borrado)
  o puntero invalido.
- Hipotesis: el truco borra los autos de policia/bloqueos de carretera; esos autos son
  vehiculos de red controlados por el host y el mod tambien los borra (o ya los borro)
  -> doble eliminacion. Fix probable: en clientes no-host, no dejar que el truco elimine
  vehiculos de red (o hacer que el truco solo actue en el host / sincronizarlo).
- Contexto: el nivel de busqueda NO se sincroniza (TODO de CoopAndreas: "wanted level sync"),
  y los policias que persiguen al host existen en la PC del amigo como peds de red.
  Hipotesis: el truco limpia/elimina policias o tareas de persecucion que en realidad
  controla el host -> estado inconsistente en peds de red.

### 5. Crash al volver al juego tras tenerlo en segundo plano (alt-tab)
- Crash DENTRO de CoopAndreasSA.dll +0xE031C (write), build 097adce (Nov 4 09:31:04),
  log 2026-11-04_07-17-26 (PC del host).
- Es el inicio de un constructor (pone a 0 campos +4,+8,+0xC,+0x48,+0x4C de `this`) y `this`
  (ECX=0x027CD100) apunta a memoria no escribible -> objeto creado sobre memoria invalida.
- Contexto: SA deja de procesar el juego cuando la ventana pierde el foco, pero el hilo de
  red de CoopAndreas sigue recibiendo paquetes; al volver se procesa todo el atraso de golpe
  (crear/borrar peds y autos de red). Hipotesis: algun create/delete en ese atraso usa un
  slot/puntero ya invalido.
- Para identificar la funcion: hacer que el CI guarde un .map/.pdb de cada build (hoy no hay
  simbolos). Fix posible: que el juego siga procesando en segundo plano mientras hay red.

## Pedidos de funciones
- Contraseña del servidor (el túnel es público).
- Panel de admin (TP, matar, expulsar, banear) — banear por ID de jugador, no por IP
  (por el túnel todos llegan con 127.x.x.x de playit).
- Guardado por jugador en la partida del host (posición, armas, vida, dinero).
- Modo ventana / ventana sin bordes como opcion en Opciones -> Pantalla (estilo SA), guardada
  en config, + que el juego siga procesando en segundo plano mientras hay conexion (arregla el
  crash #5 de alt-tab y evita que el personaje se congele para los demas). Base: CPatch.cpp ya
  tiene InitInstance_Hook que reposiciona la ventana (modo debug --coopd0/1).
- Puente CLEO + adaptación de Misterix y Dragon Ball. Skins externas. Launcher.
