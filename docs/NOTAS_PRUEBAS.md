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

### 6. Crash creando un vehiculo (modelo 422, Bobcat) sobre memoria invalida
- Crash en 0x0054F1E4 (write), log 2026-11-04_05-38-33 (PC del amigo), justo tras reabrir el juego.
- 0x54F1E0 = constructor base de entidad (pone vtable 0x863C40) llamado por la cadena
  0x542281 -> 0x532A94 -> ... desde CAutomobile::CAutomobile (0x6B0AC1 en la pila), EDI=0x1A6 (422).
- `this` (0x020C1AC0) no es escribible -> el vehiculo se construye sobre memoria invalida.
- MISMO PATRON que #4 (borrar un vehiculo que no esta en el pool) y #5 (constructor del mod
  sobre memoria invalida): apunta a un bug de manejo de memoria/pool de VEHICULOS de red
  (crear/borrar vehiculos sincronizados). Prioridad alta: probablemente un unico bug de fondo.
- Contexto: ocurrio poco despues de que el amigo activara el truco del JETPACK. El jetpack no es
  un vehiculo, pero volar rapido carga zonas nuevas -> se crean/borran muchos autos (trafico,
  generadores, autos de red del otro jugador) en poco tiempo. Sospecha: el bug aparece con
  mucho movimiento de creacion/borrado de vehiculos (alejarse/moverse rapido).

### 7. NPC y policia que "se teletransportan" cuando un jugador se aleja
- Peds (incl. policia) aparecen/desaparecen de golpe cerca del otro jugador, sin estar tan lejos.
- El trafico que se limpia al no mirar puede reaparecer delante de un jugador.
- Relacionado con como se reparte el streaming de peds/vehiculos entre host y clientes.

### Diagnostico crashes 4/5/6/7 (con mapa de simbolos del build 097adce)
- +0xE031C de CoopAndreasSA.dll = CVehicle::CVehicle(plugin::dummy) +0x2C (constructor del SDK),
  `this` invalido. #6 = mismo pero en el ctor del juego. #4 = CVehicle::operator delete con indice negativo.
- Mecanismo: CPool::Delete(ptr) con un ptr que NO pertenece al pool escribe byteMap[idx] fuera de rango
  y deja m_nFirstFree negativo; el siguiente New() devuelve &objects[negativo] -> AV en el ctor.
  Por eso los crashes parecen aleatorios (alt-tab, lejos, jetpack): disparan la siguiente creacion.
- Causa probable: `delete m_pVehicle` sobre un puntero obsoleto (IsVTableValid no detecta slots
  reutilizados) o lectura de un CNetworkVehicle ya liberado.
- Fix: guardas en operator delete de vehiculos/peds (ignorar punteros fuera del pool y loguear el
  llamador) + validar con el pool antes de borrar en CNetworkVehicle.

## Hecho (2026-11-04, segunda sesion)
- Guardas de pool (CPoolGuard): operator delete de vehiculos/peds valida el puntero; avisos `[guard]`
  en chat/consola con el llamador. Cubre #4/#5/#6/#7.
- #2 bici: GetActiveTask() nulo en 0x523E66 parcheado.
- Trucos sincronizados (CCheatSync) + aviso "X activo un truco".
- Menu Tab: seleccion con flechas, T ir al jugador; host: Y traer, K expulsar, B banear (bans.txt).
- Propiedad por cercania (server CProximityOwnership): peds/trafico aleatorios pasan al jugador mas
  cercano; el cliente restaura IA al recibirlos. `proximity_ownership = 0` lo desactiva. Spawns
  nunca a <60 m de otro jugador. (= intento de arreglar #7; a probar)
- CI: mapas de simbolos en build/symbols, publicacion con reintentos.

### 8. Crash 0x54F493 con "autos que vuelan" + jetpack (2026-11-05)
- ECX=ESI=0 en ~CPlaceable durante el desenrollado de excepcion (funclets 0x8488xx): un constructor
  de vehiculo corrio sobre NULL. El operator new del juego devuelve NULL con el pool lleno y C++ igual
  llama al constructor. Fix: CPoolGuard::EnsureFreeVehicleSlot/EnsureFreePedSlot antes de crear
  vehiculos/peds de red (libera el mas lejano propio o no crea).
- Ventana/pantalla ancha: se usan III.VC.SA.WindowedMode.asi y GTASA.WidescreenFix.asi (ThirteenAG) en scripts/.
  CWindowMode propio queda desactivado.
- Host migration funciona: al salir el host, otro jugador pasa a ser host y siguen jugando.

### 9. Crash 0x0156173D en el limite SF/LV (moto), tambien visto al host el 04/11
- EIP dentro de un trampolin HOODLUM = CColModel::AllocateData (0x40F870) con this=NULL, EDX=0x27A6:
  el pool de CColModel (10150) se lleno (CPedModelInfo::CreateHitColModelSkinned al cargar un ped).
  Fix: pool ampliado a 25000 (push en 0x551106).

## Hecho (2026-11-05)
- Radio: solo el jugador local enciende la radio del auto (0x502302).
- Morir ya no pasa 12 h ni borra vehiculos (PassTime / ClearExcitingStuffFromArea anulados en red).
- Host sincroniza ForcedWeatherType, InterpolationValue y ms_fTimeScale.
- Votacion de trucos de mundo (CCheatVote server / CCheatSync cliente / CPrompt UI), estado de trucos
  para quien entra (CheatState), TP a pedido con aceptar/rechazar (CTeleportRequest).
- Gordo/musculoso visibles para los demas (RebuildPlayer(remote, true)).
- Copiloto trabado al salir: watchdog del cambio de arma en driveby; bInModControlledDriveby solo local.

## Pendiente / a investigar
- "Copiloto como si apuntara a cada rato": revisar ApplyWeaponSnapshot y el flag driveby del passenger update.
- Avion deformado que aparece y desaparece: pedir captura/modelo; posible vehiculo de red creado antes de
  cargar el modelo o entidad con matriz corrupta.
- SensFix (punteria): pendiente de que el usuario pase el archivo.

## Pedidos de funciones
- Trucos de mundo sincronizados: policia/nivel de busqueda, cazarecompensas, autos vuelan, tiempo lento,
  etc. se aplican a todos (los personales como armas/vida/jetpack siguen siendo locales).
- Aviso "X activo un truco" visible para todos.
- Menu Tab (lista de jugadores): boton Expulsar (host).
- Submenu de teletransporte: ir a un jugador / traer a un jugador.
- Jugadores a cualquier distancia sin crashes.
- Contraseña del servidor (el túnel es público).
- Panel de admin (TP, matar, expulsar, banear) — banear por ID de jugador, no por IP
  (por el túnel todos llegan con 127.x.x.x de playit).
- Guardado por jugador en la partida del host (posición, armas, vida, dinero).
- Modo ventana / ventana sin bordes como opcion en Opciones -> Pantalla (estilo SA), guardada
  en config, + que el juego siga procesando en segundo plano mientras hay conexion (arregla el
  crash #5 de alt-tab y evita que el personaje se congele para los demas). Base: CPatch.cpp ya
  tiene InitInstance_Hook que reposiciona la ventana (modo debug --coopd0/1).
- Puente CLEO + adaptación de Misterix y Dragon Ball. Skins externas. Launcher.
