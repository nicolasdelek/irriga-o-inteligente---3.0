/* =========================================================
   IRRIGAÇÃO INTELIGENTE — ESP8266 (NodeMCU / Wemos D1 mini)
   Tudo em um arquivo: página web + API + controle dos relés.

   Ligações:
     Sensor de umidade (saída analógica) -> A0
     Relé 1 (bomba/válvula A)            -> D2 (GPIO4)
     Relé 2 (bomba/válvula B)            -> D3 (GPIO0)
   Os dois relés têm a mesma função e se revezam: o relé 1 liga
   primeiro; ao fim do tempo configurado ele desliga e o relé 2
   assume, e assim por diante. Nunca ficam ligados juntos.

   O ESP8266 CRIA a própria rede Wi-Fi (não precisa de roteador):
     1) No celular/PC, conecte na rede  AP_SSID  (senha AP_PASS)
     2) Abra no navegador:  http://192.168.4.1
   (Opcional: preencha WIFI_SSID/WIFI_PASS para ele também entrar na
   rede da sua casa; o IP dessa rede aparece no Monitor Serial.)
   ========================================================= */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <EEPROM.h>

// ---------- AJUSTE AQUI ----------
// Rede criada pelo ESP8266 (Access Point). A senha precisa ter no mínimo 8 caracteres.
const char* AP_SSID = "Irrigacao-Estufa";
const char* AP_PASS = "irrigar123";

// OPCIONAL: rede do roteador. Deixe WIFI_SSID vazio ("") para usar só a rede própria.
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

const uint8_t RELE1_PIN = D2;          // GPIO4
const uint8_t RELE2_PIN = D3;          // GPIO0 (pino de boot: veja observação abaixo)
const bool RELE_ATIVO_BAIXO = true;    // true se o módulo relé liga com sinal LOW

const int SENSOR_PIN = A0;
const int ADC_SECO = 800;              // leitura com o sensor no ar/solo seco  (calibre!)
const int ADC_MOLHADO = 350;           // leitura com o sensor na água/solo encharcado (calibre!)
// ----------------------------------

// Observação: o D3 (GPIO0) precisa estar em nível ALTO ao ligar a placa, senão ela
// entra em modo de gravação. Com módulo relé "ativo em LOW" isso é o normal (relé
// desligado = HIGH). Se a placa não iniciar, desconecte o fio do D3 e tente de novo.

// pagina.h — interface web (HTML + CSS + JS)
const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="pt-BR">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<title>Irrigação Inteligente — ESP32</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600;700&family=IBM+Plex+Sans:wght@400;500;600&family=IBM+Plex+Mono:wght@400;500&display=swap" rel="stylesheet">
<style>
:root{
--bg:            #101A14;
--bg-field:      #16241A;
--surface:       #17251C;
--surface-2:     #1E3226;
--border:        #2C4033;
--text:          #EAF0E7;
--text-dim:      #9FB6A5;
--text-faint:    #6C8375;
--water:         #4FA8D8;
--water-dim:     #2E5E77;
--soil:          #C9863F;
--soil-dim:      #7A5730;
--good:          #6FC08A;
--good-dim:      #2E4A38;
--warn:          #E3AC53;
--bad:           #E0665A;
--radius-sm: 8px;
--radius-md: 14px;
--radius-lg: 22px;
--font-display: 'Space Grotesk', sans-serif;
--font-body: 'IBM Plex Sans', sans-serif;
--font-mono: 'IBM Plex Mono', monospace;
--shadow-card: 0 1px 0 rgba(255,255,255,0.03) inset, 0 12px 30px -18px rgba(0,0,0,0.6);
}
*, *::before, *::after{ box-sizing: border-box; }
html{ scroll-behavior: smooth; }
body{
margin:0;
background: var(--bg);
color: var(--text);
font-family: var(--font-body);
line-height: 1.5;
-webkit-font-smoothing: antialiased;
}
h1,h2,h3{ font-family: var(--font-display); margin: 0 0 .4em; letter-spacing: -0.01em; }
p{ margin: 0 0 1em; color: var(--text-dim); }
button{ font-family: inherit; }
:focus-visible{ outline: 2px solid var(--water); outline-offset: 2px; }
.page{ display: none; }
.page--active{ display: block; }
#dashboard.page--active, #historico.page--active, #config.page--active, #sobre.page--active{
display: block;
animation: rise .35s ease both;
}
@keyframes rise{ from{ opacity:0; transform: translateY(6px);} to{ opacity:1; transform: translateY(0);} }
@media (prefers-reduced-motion: reduce){ .page--active{ animation: none; } }
#capa{
min-height: 100svh;
position: relative;
display:flex; align-items:center; justify-content:center;
padding: 8vh 6vw;
overflow: hidden;
background:
radial-gradient(120% 90% at 50% -10%, #1C3324 0%, var(--bg) 60%);
}
.capa__field{
position:absolute; inset:0; z-index:0;
background-image: repeating-linear-gradient(
90deg, rgba(79,168,216,0.05) 0 1px, transparent 1px 64px
), repeating-linear-gradient(
0deg, rgba(79,168,216,0.05) 0 1px, transparent 1px 64px
);
mask-image: radial-gradient(60% 60% at 50% 40%, black, transparent 80%);
}
.capa__content{
position: relative; z-index: 1;
max-width: 720px; text-align:center;
}
.capa__kicker{
font-family: var(--font-mono);
font-size: 0.8rem;
color: var(--water);
margin-bottom: 1.2em;
}
.capa__title{
font-size: clamp(2.4rem, 6vw, 4rem);
font-weight: 600;
line-height: 1.05;
}
.capa__subtitle{
font-size: 1.05rem;
max-width: 46ch;
margin: 1em auto 1.6em;
}
.capa__tags{ display:flex; gap:.5em; justify-content:center; flex-wrap:wrap; margin-bottom: 2em; }
.tag{
font-family: var(--font-mono);
font-size: .78rem;
padding: .4em .8em;
border: 1px solid var(--border);
border-radius: 999px;
color: var(--text-dim);
}
.capa__flow{
margin-top: 3em;
display:flex; flex-wrap:wrap; gap:.5em .6em; justify-content:center; align-items:center;
font-family: var(--font-mono); font-size: .8rem; color: var(--text-faint);
}
.capa__arrow{ color: var(--soil); }
.btn{
border:none; border-radius: var(--radius-sm);
padding: .75em 1.3em;
font-size: .95rem; font-weight: 600;
cursor: pointer;
transition: transform .12s ease, filter .12s ease, opacity .12s ease;
}
.btn:active{ transform: scale(0.98); }
.btn:disabled{ opacity:.4; cursor:not-allowed; }
.btn--primary{ background: var(--water); color: #08151C; }
.btn--primary:hover:not(:disabled){ filter: brightness(1.08); }
.btn--lg{ padding: .95em 2em; font-size: 1.05rem; }
.btn--ghost{ background: transparent; border: 1px solid var(--border); color: var(--text); }
.btn--ghost:hover:not(:disabled){ border-color: var(--water); }
.btn--liga{ background: var(--good); color: #0B1F13; }
.btn--liga:hover:not(:disabled){ filter: brightness(1.08); }
.btn--desliga{ background: var(--surface-2); color: var(--text); border:1px solid var(--border); }
.btn--desliga:hover:not(:disabled){ border-color: var(--bad); color: var(--bad); }
.btn--modo{ width:100%; margin-top:.7em; background: var(--soil); color:#241505; }
.btn--modo.is-active{ background: var(--water); color:#08151C; }
.app{ display:none; min-height:100svh; flex-direction:column; }
.app.app--active{ display:flex; }
.topbar{
position: sticky; top:0; z-index: 20;
display:flex; align-items:center; gap: 1.2em;
padding: .9em 1.4em;
background: rgba(16,26,20,0.9);
backdrop-filter: blur(8px);
border-bottom: 1px solid var(--border);
}
.topbar__brand{
display:flex; align-items:center; gap:.5em;
background:none; border:none; color: var(--text);
font-family: var(--font-display); font-weight:600; font-size:1rem;
cursor:pointer;
}
.topbar__leaf{ font-size:1.1rem; }
.topbar__nav{ display:flex; gap:.3em; margin-left: .5em; }
.navlink{
background:none; border:none; color: var(--text-dim);
font-size:.9rem; padding:.5em .9em; border-radius: var(--radius-sm);
cursor:pointer;
}
.navlink:hover{ color: var(--text); }
.navlink--active{ color: var(--text); background: var(--surface-2); }
.topbar__status{ margin-left:auto; display:flex; gap:.5em; }
.topbar__burger{ display:none; background:none; border:1px solid var(--border); color:var(--text); border-radius: var(--radius-sm); padding:.3em .6em; margin-left:.5em; }
.pill{
display:flex; align-items:center; gap:.45em;
font-family: var(--font-mono); font-size:.78rem; color: var(--text-dim);
border:1px solid var(--border); border-radius:999px; padding:.35em .8em;
}
.dot{ width:8px; height:8px; border-radius:50%; background: var(--text-faint); display:inline-block; }
.pill--ok .dot{ background: var(--good); box-shadow: 0 0 8px var(--good); }
.pill--bad .dot{ background: var(--bad); }
@media (max-width: 820px){
.topbar__nav{
position: fixed; inset: 56px 0 auto 0; z-index:19;
background: var(--bg-field);
border-bottom: 1px solid var(--border);
flex-direction:column; padding: .6em;
display:none;
}
.topbar__nav.is-open{ display:flex; }
.navlink{ text-align:left; width:100%; }
.topbar__burger{ display:inline-block; }
.topbar__status .pill span{ display:none; }
}
.toast-stack{
position: fixed; top: 76px; right: 16px; z-index: 50;
display:flex; flex-direction:column; gap:.5em;
max-width: min(340px, 90vw);
}
.toast{
font-size:.88rem;
padding:.75em 1em;
border-radius: var(--radius-sm);
background: var(--surface-2);
border:1px solid var(--border);
border-left: 3px solid var(--good);
box-shadow: var(--shadow-card);
animation: toast-in .2s ease both;
}
.toast--warn{ border-left-color: var(--warn); }
.toast--bad{ border-left-color: var(--bad); }
@keyframes toast-in{ from{ opacity:0; transform: translateX(12px);} to{ opacity:1; transform:none; } }
.banner{
margin: 1em 1.4em 0;
padding: .8em 1.1em;
background: rgba(224,101,90,0.12);
border: 1px solid var(--bad);
color: #F2B5AE;
border-radius: var(--radius-sm);
font-size: .9rem;
}
.grid-dash{
padding: 1.4em;
display: grid;
gap: 1em;
grid-template-columns: repeat(12, 1fr);
}
.card{
background: var(--surface);
border: 1px solid var(--border);
border-radius: var(--radius-lg);
padding: 1.3em;
box-shadow: var(--shadow-card);
}
.card__head{ display:flex; justify-content:space-between; align-items:center; }
.card--umidade{ grid-column: span 5; }
.card--irrigacao{ grid-column: span 3; }
.card--controles{ grid-column: span 4; }
.card--auto{ grid-column: span 6; }
.card--grafico{ grid-column: span 6; }
.card--rele2{ grid-column: span 12; }
.card--eventos{ grid-column: span 12; }
@media (max-width: 980px){
.card--umidade, .card--irrigacao, .card--controles, .card--auto, .card--grafico, .card--eventos, .card--rele2{
grid-column: span 12;
}
}
.sensor-flag{
font-family: var(--font-mono); font-size: .72rem;
padding: .3em .7em; border-radius: 999px;
background: var(--good-dim); color: var(--good);
}
.sensor-flag.is-erro{ background: rgba(224,101,90,0.15); color: var(--bad); }
.umidade__value{
font-family: var(--font-display); font-weight:700;
font-size: clamp(2.6rem, 6vw, 3.6rem);
margin: .2em 0 .3em;
color: var(--water);
}
.umidade__bar{
position: relative;
height: 14px; border-radius: 999px;
background: var(--surface-2);
overflow: visible;
margin-top: .6em;
}
.umidade__fill{
height:100%; border-radius:999px;
background: linear-gradient(90deg, var(--soil), var(--water));
transition: width .5s ease;
}
.umidade__marker{
position:absolute; top:-4px; width:2px; height:22px;
background: var(--text-faint);
}
.umidade__marker::after{
content:''; position:absolute; bottom:-6px; left:-3px;
width:8px; height:8px; border-radius:50%; background: inherit;
}
.umidade__legend{ display:flex; justify-content:space-between; font-size:.75rem; color: var(--text-faint); margin-top:.5em; }
.irrigacao__state{
font-family: var(--font-display); font-weight:600;
font-size: 1.6rem;
padding: .4em .2em;
color: var(--text-dim);
}
.card--irrigacao.is-on .irrigacao__state{ color: var(--good); }
.irrigacao__modo{ font-size:.9rem; color: var(--text-dim); }
.rev__row{ display:grid; grid-template-columns:1fr 1fr; gap:.8em; margin:.6em 0; }
.rev__rele{
display:flex; flex-direction:column; gap:.2em; padding:.9em 1em;
border:1px solid var(--border); border-radius: var(--radius-md);
background: var(--surface-2); color: var(--text-dim);
}
.rev__rele small{ font-family: var(--font-mono); color: var(--text-faint); }
.rev__rele.is-on{ border-color: var(--good); color: var(--good); box-shadow: 0 0 0 1px var(--good) inset; }
.rev__rele.is-on small{ color: var(--good); }
.rele2__timer{ font-family: var(--font-mono); font-size:2rem; color: var(--text-dim); margin-bottom:.4em; }
.card--rele2.is-on .rele2__timer{ color: var(--good); }
.controles__row{ display:flex; gap:.6em; flex-wrap:wrap; }
.controles__row .btn{ flex:1 1 auto; }
.controles__hint{ font-size:.82rem; margin-top:.8em; margin-bottom:0; color: var(--text-faint); }
.field{ display:block; margin-bottom: 1em; }
.field > span{ display:block; font-size:.85rem; color: var(--text-dim); margin-bottom:.4em; }
.field__row{ display:flex; align-items:center; gap:.8em; }
input[type="range"]{ flex:1; accent-color: var(--water); }
output, .field input[type="number"], .field input[type="text"], .field select{
font-family: var(--font-mono);
}
.field input[type="number"], .field input[type="text"], .field select{
width:100%; background: var(--surface-2); border:1px solid var(--border);
color: var(--text); border-radius: var(--radius-sm); padding:.6em .8em;
}
.field--toggle{ display:flex; align-items:center; justify-content:space-between; }
.field--toggle > span{ margin:0; }
canvas{ width:100%; display:block; }
.eventos-lista{ list-style:none; margin:0; padding:0; display:flex; flex-direction:column; gap:.5em; }
.evento{
display:flex; justify-content:space-between; gap:1em;
font-size:.88rem; padding:.6em .2em; border-bottom: 1px solid var(--border);
}
.evento:last-child{ border-bottom:none; }
.evento__time{ font-family: var(--font-mono); color: var(--text-faint); }
.evento--vazio{ color: var(--text-faint); justify-content:center; }
.page-inner{ max-width: 980px; margin: 0 auto; padding: 2.4em 1.4em 4em; }
.page-inner--narrow{ max-width: 720px; }
.page-lead{ font-size: 1.05rem; }
.lista-check{ padding-left: 1.2em; color: var(--text-dim); }
.lista-check li{ margin-bottom:.5em; }
.tabela{ width:100%; border-collapse: collapse; margin-top: 1.5em; font-size:.9rem; }
.tabela th, .tabela td{ text-align:left; padding: .7em .6em; border-bottom: 1px solid var(--border); }
.tabela th{ color: var(--text-faint); font-weight:500; font-family: var(--font-mono); font-size:.78rem; }
.tabela__vazio{ text-align:center; color: var(--text-faint); }
.form-card{
background: var(--surface); border:1px solid var(--border);
border-radius: var(--radius-lg); padding: 1.4em; margin-top: 1.5em;
}
.form-card__title{ font-size:1.1rem; }
.form-card__actions{ margin-top: .4em; }
.footer{
text-align:center; padding: 2em 1em 3em;
color: var(--text-faint); font-size:.8rem;
}
</style>
</head>
<body>
<section id="capa" class="page page--active">
<div class="capa__field" aria-hidden="true"></div>
<div class="capa__content">
<p class="capa__kicker">Trabalho de Conclusão de Curso</p>
<h1 class="capa__title">Sistema de Irrigação<br>Inteligente</h1>
<p class="capa__subtitle">Controle e monitoramento da irrigação de uma estufa utilizando ESP32, sensor de umidade do solo e uma interface web responsiva.</p>
<div class="capa__tags">
<span class="tag">ESP32</span>
<span class="tag">Wi-Fi</span>
<span class="tag">Automação</span>
<span class="tag">Tempo real</span>
</div>
<button class="btn btn--primary btn--lg" id="btn-acessar">Acessar sistema</button>
<div class="capa__flow" aria-label="Fluxo do sistema">
<span>Sensor</span><span class="capa__arrow">→</span>
<span>ESP32</span><span class="capa__arrow">→</span>
<span>Wi-Fi</span><span class="capa__arrow">→</span>
<span>Interface</span><span class="capa__arrow">→</span>
<span>Relé</span><span class="capa__arrow">→</span>
<span>Bomba</span>
</div>
</div>
</section>
<div id="app" class="app">
<header class="topbar">
<button class="topbar__brand" data-nav="dashboard">
<span class="topbar__leaf">🌱</span>
<span>Irrigação Inteligente</span>
</button>
<nav class="topbar__nav" id="main-nav">
<button class="navlink navlink--active" data-nav="dashboard">Painel</button>
<button class="navlink" data-nav="historico">Histórico</button>
<button class="navlink" data-nav="config">Configurações</button>
<button class="navlink" data-nav="sobre">Sobre</button>
</nav>
<div class="topbar__status">
<span class="pill" id="pill-esp32"><i class="dot"></i>ESP32</span>
<span class="pill" id="pill-wifi"><i class="dot"></i>Wi-Fi</span>
</div>
<button class="topbar__burger" id="burger" aria-label="Abrir menu">☰</button>
</header>
<div class="toast-stack" id="toast-stack" aria-live="polite"></div>
<main class="page page--active" id="dashboard" data-page="dashboard">
<div class="banner" id="offline-banner" hidden>
ESP32 desconectado — os comandos estão desativados até a reconexão.
</div>
<section class="grid-dash">
<article class="card card--umidade">
<div class="card__head">
<h2>Umidade do solo</h2>
<span class="sensor-flag" id="sensor-flag">normal</span>
</div>
<div class="umidade__value" id="umidade-valor">--%</div>
<div class="umidade__bar">
<div class="umidade__fill" id="umidade-fill" style="width:0%"></div>
<div class="umidade__marker umidade__marker--min" id="marker-min" style="left:40%"></div>
<div class="umidade__marker umidade__marker--max" id="marker-max" style="left:60%"></div>
</div>
<div class="umidade__legend">
<span>Seco</span><span>Úmido</span>
</div>
</article>
<article class="card card--irrigacao" id="card-irrigacao">
<h2>Irrigação</h2>
<div class="irrigacao__state" id="irrigacao-estado">Desligada</div>
<div class="irrigacao__modo">
Modo <strong id="modo-label">Manual</strong>
</div>
</article>
<article class="card card--controles">
<h2>Controles</h2>
<div class="controles__row">
<button class="btn btn--liga" id="btn-ligar">Ligar irrigação</button>
<button class="btn btn--desliga" id="btn-desligar">Desligar irrigação</button>
</div>
<button class="btn btn--modo" id="btn-modo">
<span id="btn-modo-label">Ativar modo automático</span>
</button>
<p class="controles__hint" id="controles-hint">No modo manual, você controla a bomba diretamente pelos botões acima.</p>
</article>
<article class="card card--auto">
<h2>Modo automático</h2>
<label class="field">
<span>Umidade mínima — liga a bomba abaixo deste valor</span>
<div class="field__row">
<input type="range" id="range-min" min="0" max="100" value="40">
<output id="out-min">40%</output>
</div>
</label>
<label class="field">
<span>Umidade desejada — desliga a bomba ao atingir</span>
<div class="field__row">
<input type="range" id="range-max" min="0" max="100" value="60">
<output id="out-max">60%</output>
</div>
</label>
<button class="btn btn--ghost" id="btn-salvar-limites">Salvar limites</button>
</article>
<article class="card card--grafico">
<div class="card__head">
<h2>Umidade nas últimas horas</h2>
</div>
<canvas id="grafico-umidade" height="160"></canvas>
</article>
<article class="card card--rele2" id="card-revezamento">
<div class="card__head">
<h2>Revezamento dos relés</h2>
<span class="sensor-flag" id="rev-flag">parado</span>
</div>
<p id="rev-desc">Com a irrigação ligada, os relés se revezam a cada intervalo.</p>
<div class="rev__row">
<div class="rev__rele" id="rev-1"><strong>Relé 1</strong><small>D2 · <span>aguardando</span></small></div>
<div class="rev__rele" id="rev-2"><strong>Relé 2</strong><small>D3 · <span>aguardando</span></small></div>
</div>
<div class="rele2__timer" id="rev-timer">--:--</div>
<p class="controles__hint">O relé 1 liga primeiro; ao fim do intervalo ele desliga e o relé 2 assume. Os dois nunca ficam ligados ao mesmo tempo.</p>
</article>
<article class="card card--eventos">
<h2>Últimos eventos</h2>
<ul class="eventos-lista" id="eventos-lista">
<li class="evento evento--vazio">Nenhum evento registrado ainda.</li>
</ul>
</article>
</section>
</main>
<main class="page" id="historico" data-page="historico">
<section class="page-inner">
<h1>Histórico de irrigação</h1>
<p class="page-lead">Registro de cada acionamento e desligamento da bomba, com horário, modo e umidade no momento.</p>
<table class="tabela" id="tabela-historico">
<thead>
<tr><th>Horário</th><th>Ação</th><th>Modo</th><th>Umidade</th></tr>
</thead>
<tbody id="tabela-historico-body">
<tr><td colspan="4" class="tabela__vazio">Sem registros por enquanto.</td></tr>
</tbody>
</table>
</section>
</main>
<main class="page" id="config" data-page="config">
<section class="page-inner page-inner--narrow">
<h1>Configurações</h1>
<p class="page-lead">Estes valores são enviados ao ESP32 e guardados na memória do dispositivo, permanecendo após reiniciar.</p>
<div class="form-card">
<label class="field">
<span>Nome da estufa</span>
<input type="text" id="cfg-nome" placeholder="Estufa 1">
</label>
<label class="field">
<span>Umidade mínima (%)</span>
<input type="number" id="cfg-min" min="0" max="100" value="40">
</label>
<label class="field">
<span>Umidade desejada (%)</span>
<input type="number" id="cfg-max" min="0" max="100" value="60">
</label>
<label class="field">
<span>Intervalo de atualização (segundos)</span>
<input type="number" id="cfg-intervalo" min="1" max="60" value="2">
</label>
<label class="field">
<span>Tempo de cada relé no revezamento (minutos)</span>
<input type="number" id="cfg-rev-min" min="1" max="120" value="3">
</label>
<label class="field">
<span>Modo padrão ao iniciar</span>
<select id="cfg-modo-padrao">
<option value="manual">Manual</option>
<option value="automatico">Automático</option>
</select>
</label>
<div class="form-card__actions">
<button class="btn btn--primary" id="btn-salvar-config">Salvar configurações</button>
</div>
</div>
<div class="form-card">
<h2 class="form-card__title">Conexão com o ESP32</h2>
<label class="field">
<span>Endereço IP do ESP32</span>
<input type="text" id="cfg-ip" placeholder="192.168.1.50">
</label>
<label class="field field--toggle">
<span>Modo simulação (testar sem hardware)</span>
<input type="checkbox" id="cfg-simulacao">
</label>
<div class="form-card__actions">
<button class="btn btn--ghost" id="btn-salvar-ip">Salvar conexão</button>
</div>
</div>
</section>
</main>
<main class="page" id="sobre" data-page="sobre">
<section class="page-inner page-inner--narrow">
<h1>Sobre o projeto</h1>
<p class="page-lead">A irrigação manual em pequenas estufas costuma depender da disponibilidade e da memória de quem cuida das plantas — o que leva a solos encharcados, plantas ressecadas e desperdício de água.</p>
<h2>A proposta</h2>
<p>Este projeto automatiza a irrigação com um ESP32 que lê continuamente um sensor de umidade do solo e decide, dentro de limites configuráveis, quando ligar e desligar uma bomba d'água de 12 V através de um módulo relé.</p>
<h2>Como funciona</h2>
<p>O ESP32 conecta-se à rede Wi-Fi local e disponibiliza uma API HTTP própria. A interface web consulta essa API periodicamente e envia comandos de controle, sem depender de nenhum servidor externo.</p>
<h2>Benefícios</h2>
<ul class="lista-check">
<li>Economia de água, evitando irrigação além do necessário</li>
<li>Praticidade: liga e desliga a bomba automaticamente</li>
<li>Monitoramento remoto pelo celular ou computador</li>
<li>Histórico do que aconteceu enquanto ninguém estava por perto</li>
</ul>
</section>
</main>
<footer class="footer">
<span>Sistema de Irrigação Inteligente — TCC · ESP32</span>
</footer>
</div>
<script>
const STORAGE_KEY = "irrigacao-config";
const EMBEDDED = true;   // true = página servida pelo próprio ESP8266 (mesma origem)
const defaultConfig = {
ip: "192.168.1.50",
simulation: !EMBEDDED,          // true = interface roda sem hardware real
intervalo: 2,               // segundos entre consultas de status
umidadeMin: 40,
umidadeMax: 60,
nomeEstufa: "Estufa 1",
modoPadrao: "manual",
revezamentoMinutos: 3,      // tempo de cada relé antes de passar para o outro
};
function loadConfig() {
try {
const raw = localStorage.getItem(STORAGE_KEY);
if (!raw) return { ...defaultConfig };
return { ...defaultConfig, ...JSON.parse(raw) };
} catch (e) {
return { ...defaultConfig };
}
}
function saveConfig(cfg) {
localStorage.setItem(STORAGE_KEY, JSON.stringify(cfg));
}
let config = loadConfig();
const state = {
online: false,
wifi: false,
umidade: null,
irrigando: false,
modo: config.modoPadrao,      // "manual" | "automatico"
sensorErro: false,
historicoUmidade: [],          // { t: Date, valor: number }
historicoEventos: [],          // { t, acao, modo, umidade }
pollTimer: null,
commandInFlight: false,
releAtivo: 0,                 // 0 = nenhum, 1 ou 2
releFim: null,                // timestamp (ms) da próxima troca
};
const sim = {
umidade: 55,
bomba: false,
rele: 0,
fim: 0,
};
function simLigar() {
sim.bomba = true;
sim.rele = 1;
sim.fim = Date.now() + config.revezamentoMinutos * 60000;
}
function simDesligar() {
sim.bomba = false;
sim.rele = 0;
}
function simularCiclo() {
if (sim.bomba && Date.now() >= sim.fim) {
sim.rele = sim.rele === 1 ? 2 : 1;
sim.fim = Date.now() + config.revezamentoMinutos * 60000;
registrarEvento(`Relé ${sim.rele} ligado (revezamento)`, Math.round(sim.umidade));
}
const drift = sim.bomba ? (Math.random() * 2.2 + 0.6) : -(Math.random() * 0.7 + 0.15);
sim.umidade = Math.max(0, Math.min(100, sim.umidade + drift));
if (state.modo === "automatico") {
if (sim.umidade < config.umidadeMin && !sim.bomba) {
simLigar();
registrarEvento("Irrigação ligada", Math.round(sim.umidade));
} else if (sim.umidade >= config.umidadeMax && sim.bomba) {
simDesligar();
registrarEvento("Irrigação desligada", Math.round(sim.umidade));
}
}
return {
umidade: Math.round(sim.umidade),
irrigacao: sim.bomba,
modo: state.modo,
wifi: true,
sensor_ok: true,
rele_ativo: sim.rele,
rele_restante: sim.bomba ? Math.max(0, Math.round((sim.fim - Date.now()) / 1000)) : null,
};
}
async function chamarApi(caminho, opts = {}) {
const url = EMBEDDED ? caminho : `http://${config.ip}${caminho}`;
const res = await fetch(url, { ...opts, signal: AbortSignal.timeout(3000) });
if (!res.ok) throw new Error(`HTTP ${res.status}`);
return res.json();
}
async function buscarStatus() {
if (config.simulation) {
return simularCiclo();
}
return chamarApi("/api/status");
}
async function enviarComando(caminho, body) {
if (config.simulation) {
if (caminho === "/api/ligar") { simLigar(); registrarEvento("Irrigação ligada (manual)", Math.round(sim.umidade)); }
if (caminho === "/api/desligar") { simDesligar(); registrarEvento("Irrigação desligada (manual)", Math.round(sim.umidade)); }
if (caminho === "/api/automatico") state.modo = "automatico";
if (caminho === "/api/manual") state.modo = "manual";
return { ok: true };
}
return chamarApi(caminho, {
method: "POST",
headers: { "Content-Type": "application/json" },
body: body ? JSON.stringify(body) : undefined,
});
}
async function atualizarStatus() {
try {
const data = await buscarStatus();
state.online = true;
state.wifi = !!data.wifi;
state.sensorErro = data.sensor_ok === false || data.umidade < 0 || data.umidade > 100;
state.umidade = state.sensorErro ? state.umidade : data.umidade;
state.irrigando = !!data.irrigacao;
state.modo = data.modo || state.modo;
if (EMBEDDED && !state.sincronizado && data.umidade_min != null) {
config.umidadeMin = data.umidade_min;
config.umidadeMax = data.umidade_max;
if (data.revezamento_min) config.revezamentoMinutos = data.revezamento_min;
saveConfig(config);
preencherFormularios();
state.sincronizado = true;
}
state.releAtivo = state.irrigando ? (data.rele_ativo || 0) : 0;
state.releFim = (state.releAtivo && data.rele_restante != null) ? Date.now() + data.rele_restante * 1000 : null;
if (!state.sensorErro && typeof state.umidade === "number") {
state.historicoUmidade.push({ t: new Date(), valor: state.umidade });
if (state.historicoUmidade.length > 60) state.historicoUmidade.shift();
}
} catch (e) {
state.online = false;
}
render();
}
function iniciarPolling() {
if (state.pollTimer) clearInterval(state.pollTimer);
atualizarStatus();
state.pollTimer = setInterval(atualizarStatus, Math.max(1, config.intervalo) * 1000);
}
function registrarEvento(acao, umidade) {
const evento = { t: new Date(), acao, modo: state.modo, umidade };
state.historicoEventos.unshift(evento);
if (state.historicoEventos.length > 100) state.historicoEventos.pop();
renderEventos();
renderHistoricoTabela();
}
function formatHora(d) {
return d.toLocaleTimeString("pt-BR", { hour: "2-digit", minute: "2-digit" });
}
function toast(msg, tipo = "ok") {
const stack = document.getElementById("toast-stack");
const el = document.createElement("div");
el.className = `toast ${tipo === "warn" ? "toast--warn" : tipo === "bad" ? "toast--bad" : ""}`;
el.textContent = msg;
stack.appendChild(el);
setTimeout(() => el.remove(), 4200);
}
function render() {
const pillEsp = document.getElementById("pill-esp32");
const pillWifi = document.getElementById("pill-wifi");
pillEsp.className = `pill ${state.online ? "pill--ok" : "pill--bad"}`;
pillEsp.innerHTML = `<i class="dot"></i><span>ESP32 ${state.online ? "conectado" : "desconectado"}</span>`;
pillWifi.className = `pill ${state.wifi ? "pill--ok" : "pill--bad"}`;
pillWifi.innerHTML = `<i class="dot"></i><span>Wi-Fi ${state.wifi ? "conectado" : "desconectado"}</span>`;
document.getElementById("offline-banner").hidden = state.online;
const valor = state.umidade ?? 0;
document.getElementById("umidade-valor").textContent = state.umidade === null ? "--%" : `${valor}%`;
document.getElementById("umidade-fill").style.width = `${valor}%`;
document.getElementById("marker-min").style.left = `${config.umidadeMin}%`;
document.getElementById("marker-max").style.left = `${config.umidadeMax}%`;
const flag = document.getElementById("sensor-flag");
flag.textContent = state.sensorErro ? "erro no sensor" : "normal";
flag.classList.toggle("is-erro", state.sensorErro);
const card = document.getElementById("card-irrigacao");
card.classList.toggle("is-on", state.irrigando);
document.getElementById("irrigacao-estado").textContent = state.irrigando ? "Irrigando" : "Desligada";
document.getElementById("modo-label").textContent = state.modo === "automatico" ? "Automático" : "Manual";
const modoAuto = state.modo === "automatico";
document.getElementById("btn-modo").classList.toggle("is-active", modoAuto);
document.getElementById("btn-modo-label").textContent = modoAuto ? "Voltar ao modo manual" : "Ativar modo automático";
document.getElementById("btn-ligar").disabled = !state.online || modoAuto || state.irrigando;
document.getElementById("btn-desligar").disabled = !state.online || modoAuto || !state.irrigando;
document.getElementById("controles-hint").textContent = modoAuto
? "O modo automático está ativo: o ESP32 liga e desliga a bomba sozinho, conforme os limites configurados."
: "No modo manual, você controla a bomba diretamente pelos botões acima.";
renderRevezamento();
desenharGrafico();
}
function renderRevezamento() {
const ativo = state.releAtivo;
document.getElementById("card-revezamento").classList.toggle("is-on", !!ativo);
document.getElementById("rev-flag").textContent = ativo ? `relé ${ativo} ligado` : "parado";
document.getElementById("rev-desc").textContent = ativo
? `Cada relé fica ligado ${config.revezamentoMinutos} min. Próxima troca em:`
: "Com a irrigação ligada, os relés se revezam a cada intervalo.";
[1, 2].forEach(n => {
const el = document.getElementById(`rev-${n}`);
el.classList.toggle("is-on", ativo === n);
el.querySelector("small span").textContent = ativo === n ? "ligado" : "aguardando";
});
renderRevTimer();
}
function renderRevTimer() {
const el = document.getElementById("rev-timer");
if (!state.releAtivo || !state.releFim) { el.textContent = "--:--"; return; }
const s = Math.max(0, Math.round((state.releFim - Date.now()) / 1000));
el.textContent = `${String(Math.floor(s / 60)).padStart(2, "0")}:${String(s % 60).padStart(2, "0")}`;
}
function renderEventos() {
const lista = document.getElementById("eventos-lista");
if (state.historicoEventos.length === 0) {
lista.innerHTML = `<li class="evento evento--vazio">Nenhum evento registrado ainda.</li>`;
return;
}
lista.innerHTML = state.historicoEventos.slice(0, 6).map(ev => `
<li class="evento">
<span>${ev.acao}</span>
<span class="evento__time">${formatHora(ev.t)} · ${ev.umidade}%</span>
</li>
`).join("");
}
function renderHistoricoTabela() {
const body = document.getElementById("tabela-historico-body");
if (state.historicoEventos.length === 0) {
body.innerHTML = `<tr><td colspan="4" class="tabela__vazio">Sem registros por enquanto.</td></tr>`;
return;
}
body.innerHTML = state.historicoEventos.map(ev => `
<tr>
<td>${formatHora(ev.t)}</td>
<td>${ev.acao}</td>
<td>${ev.modo === "automatico" ? "Automático" : "Manual"}</td>
<td>${ev.umidade}%</td>
</tr>
`).join("");
}
function desenharGrafico() {
const canvas = document.getElementById("grafico-umidade");
const ctx = canvas.getContext("2d");
const dpr = window.devicePixelRatio || 1;
const cssWidth = canvas.clientWidth || 400;
const cssHeight = 160;
canvas.width = cssWidth * dpr;
canvas.height = cssHeight * dpr;
ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
ctx.clearRect(0, 0, cssWidth, cssHeight);
const pontos = state.historicoUmidade;
const pad = 24;
ctx.strokeStyle = "rgba(159,182,165,0.15)";
ctx.lineWidth = 1;
[0, 25, 50, 75, 100].forEach(pct => {
const y = pad + (1 - pct / 100) * (cssHeight - pad * 1.4);
ctx.beginPath();
ctx.moveTo(pad, y);
ctx.lineTo(cssWidth - 8, y);
ctx.stroke();
});
if (pontos.length < 2) {
ctx.fillStyle = "#6C8375";
ctx.font = "13px 'IBM Plex Sans', sans-serif";
ctx.fillText("Aguardando leituras...", pad, cssHeight / 2);
return;
}
const xStep = (cssWidth - pad - 8) / (pontos.length - 1);
const yFor = (v) => pad + (1 - v / 100) * (cssHeight - pad * 1.4);
ctx.beginPath();
pontos.forEach((p, i) => {
const x = pad + i * xStep;
const y = yFor(p.valor);
if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
});
ctx.strokeStyle = "#4FA8D8";
ctx.lineWidth = 2;
ctx.stroke();
ctx.lineTo(pad + (pontos.length - 1) * xStep, cssHeight - pad * 0.4);
ctx.lineTo(pad, cssHeight - pad * 0.4);
ctx.closePath();
ctx.fillStyle = "rgba(79,168,216,0.12)";
ctx.fill();
}
function irPara(pagina) {
document.querySelectorAll(".app > .page").forEach(p => p.classList.toggle("page--active", p.dataset.page === pagina));
document.querySelectorAll(".navlink").forEach(n => n.classList.toggle("navlink--active", n.dataset.nav === pagina));
document.getElementById("main-nav").classList.remove("is-open");
window.scrollTo({ top: 0, behavior: "instant" in window ? "instant" : "auto" });
}
async function ligar() {
if (state.commandInFlight) return;
state.commandInFlight = true;
try {
await enviarComando("/api/ligar");
toast("Irrigação ligada com sucesso.");
await atualizarStatus();
} catch (e) {
toast("Não foi possível ligar a bomba.", "bad");
} finally {
state.commandInFlight = false;
}
}
async function desligar() {
if (state.commandInFlight) return;
state.commandInFlight = true;
try {
await enviarComando("/api/desligar");
toast("Irrigação desligada com sucesso.");
await atualizarStatus();
} catch (e) {
toast("Não foi possível desligar a bomba.", "bad");
} finally {
state.commandInFlight = false;
}
}
async function alternarModo() {
const novoModo = state.modo === "automatico" ? "manual" : "automatico";
try {
await enviarComando(novoModo === "automatico" ? "/api/automatico" : "/api/manual");
state.modo = novoModo;
toast(novoModo === "automatico" ? "Modo automático ativado." : "Modo manual ativado.");
await atualizarStatus();
} catch (e) {
toast("Falha na comunicação com o ESP32.", "bad");
}
}
async function salvarLimites() {
config.umidadeMin = Number(document.getElementById("range-min").value);
config.umidadeMax = Number(document.getElementById("range-max").value);
saveConfig(config);
try {
await enviarComando("/api/config", { umidadeMin: config.umidadeMin, umidadeMax: config.umidadeMax });
toast("Limites de umidade salvos.");
} catch (e) {
toast("Limites salvos localmente, mas o ESP32 não confirmou.", "warn");
}
render();
}
function salvarConfiguracoes() {
config.nomeEstufa = document.getElementById("cfg-nome").value || defaultConfig.nomeEstufa;
config.umidadeMin = Number(document.getElementById("cfg-min").value);
config.umidadeMax = Number(document.getElementById("cfg-max").value);
config.intervalo = Number(document.getElementById("cfg-intervalo").value) || defaultConfig.intervalo;
config.modoPadrao = document.getElementById("cfg-modo-padrao").value;
config.revezamentoMinutos = Math.max(1, Number(document.getElementById("cfg-rev-min").value) || defaultConfig.revezamentoMinutos);
saveConfig(config);
enviarComando("/api/config", { revezamentoMinutos: config.revezamentoMinutos, umidadeMin: config.umidadeMin, umidadeMax: config.umidadeMax, modoPadrao: config.modoPadrao })
.catch(() => toast("Salvo localmente, mas o ESP32 não confirmou o intervalo.", "warn"));
toast("Configurações salvas.");
iniciarPolling();
}
function salvarConexao() {
config.ip = document.getElementById("cfg-ip").value.trim() || defaultConfig.ip;
config.simulation = document.getElementById("cfg-simulacao").checked;
saveConfig(config);
toast(config.simulation ? "Modo simulação ativado." : `Tentando conectar ao ESP32 em ${config.ip}...`);
iniciarPolling();
}
function preencherFormularios() {
document.getElementById("range-min").value = config.umidadeMin;
document.getElementById("range-max").value = config.umidadeMax;
document.getElementById("out-min").textContent = `${config.umidadeMin}%`;
document.getElementById("out-max").textContent = `${config.umidadeMax}%`;
document.getElementById("cfg-nome").value = config.nomeEstufa;
document.getElementById("cfg-min").value = config.umidadeMin;
document.getElementById("cfg-max").value = config.umidadeMax;
document.getElementById("cfg-intervalo").value = config.intervalo;
document.getElementById("cfg-modo-padrao").value = config.modoPadrao;
document.getElementById("cfg-rev-min").value = config.revezamentoMinutos;
document.getElementById("cfg-ip").value = config.ip;
document.getElementById("cfg-simulacao").checked = config.simulation;
}
function ligarEventos() {
document.getElementById("btn-acessar").addEventListener("click", () => {
document.getElementById("capa").classList.remove("page--active");
document.getElementById("app").classList.add("app--active");
iniciarPolling();
});
document.querySelectorAll("[data-nav]").forEach(el => {
el.addEventListener("click", () => irPara(el.dataset.nav));
});
document.getElementById("burger").addEventListener("click", () => {
document.getElementById("main-nav").classList.toggle("is-open");
});
document.getElementById("btn-ligar").addEventListener("click", ligar);
document.getElementById("btn-desligar").addEventListener("click", desligar);
document.getElementById("btn-modo").addEventListener("click", alternarModo);
setInterval(renderRevTimer, 1000);
const rangeMin = document.getElementById("range-min");
const rangeMax = document.getElementById("range-max");
rangeMin.addEventListener("input", () => document.getElementById("out-min").textContent = `${rangeMin.value}%`);
rangeMax.addEventListener("input", () => document.getElementById("out-max").textContent = `${rangeMax.value}%`);
document.getElementById("btn-salvar-limites").addEventListener("click", salvarLimites);
document.getElementById("btn-salvar-config").addEventListener("click", salvarConfiguracoes);
document.getElementById("btn-salvar-ip").addEventListener("click", salvarConexao);
window.addEventListener("resize", desenharGrafico);
}
document.addEventListener("DOMContentLoaded", () => {
preencherFormularios();
ligarEventos();
});
</script>
</body>
</html>
)rawliteral";


ESP8266WebServer server(80);

struct Config {
  uint32_t magic;
  uint8_t  umidMin;
  uint8_t  umidMax;
  uint8_t  modoPadrao;   // 0 = manual, 1 = automático
  uint16_t revMin;       // minutos que cada relé fica ligado
};
const uint32_t MAGIC = 0x1F2E3D01;
Config cfg;

bool irrigando = false;
bool modoAuto = false;
int  releAtivo = 0;                    // 0 = nenhum, 1 ou 2
unsigned long releFim = 0;
int  umidade = 0;
bool sensorOk = true;
unsigned long ultimaLeitura = 0;

void salvarConfig() { EEPROM.put(0, cfg); EEPROM.commit(); }

void carregarConfig() {
  EEPROM.begin(64);
  EEPROM.get(0, cfg);
  if (cfg.magic != MAGIC) {
    cfg = Config{MAGIC, 40, 60, 0, 3};
    salvarConfig();
  }
}

void aplicarReles() {
  digitalWrite(RELE1_PIN, ((releAtivo == 1) ^ RELE_ATIVO_BAIXO) ? HIGH : LOW);
  digitalWrite(RELE2_PIN, ((releAtivo == 2) ^ RELE_ATIVO_BAIXO) ? HIGH : LOW);
}

void iniciarIrrigacao() {
  irrigando = true;
  releAtivo = 1;                                   // o relé 1 liga primeiro
  releFim = millis() + (unsigned long)cfg.revMin * 60000UL;
  aplicarReles();
}

void pararIrrigacao() {
  irrigando = false;
  releAtivo = 0;
  aplicarReles();
}

void lerSensor() {
  long soma = 0;
  for (int i = 0; i < 8; i++) { soma += analogRead(SENSOR_PIN); delay(2); }
  int raw = soma / 8;
  sensorOk = (raw > 2 && raw < 1022);              // 0 ou 1023 cravado = sensor solto/em curto
  if (sensorOk) umidade = constrain(map(raw, ADC_SECO, ADC_MOLHADO, 0, 100), 0, 100);
}

void logicaAutomatica() {
  if (!modoAuto) return;
  if (!sensorOk) { if (irrigando) pararIrrigacao(); return; }   // segurança: sem leitura, não irriga
  if (!irrigando && umidade < cfg.umidMin) iniciarIrrigacao();
  else if (irrigando && umidade >= cfg.umidMax) pararIrrigacao();
}

void revezarReles() {
  if (irrigando && (long)(millis() - releFim) >= 0) {
    releAtivo = (releAtivo == 1) ? 2 : 1;
    releFim = millis() + (unsigned long)cfg.revMin * 60000UL;
    aplicarReles();
  }
}

/* ---------- API ---------- */
void enviarJson(const String& s) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", s);
}

void handleStatus() {
  long restante = irrigando ? max(0L, (long)(releFim - millis()) / 1000) : -1;
  String j = "{";
  j += "\"umidade\":" + String(umidade);
  j += ",\"irrigacao\":" + String(irrigando ? "true" : "false");
  j += ",\"modo\":\"" + String(modoAuto ? "automatico" : "manual") + "\"";
  j += ",\"wifi\":true";                       // a rede própria (AP) está sempre ativa
  j += ",\"sensor_ok\":" + String(sensorOk ? "true" : "false");
  j += ",\"rele_ativo\":" + String(releAtivo);
  j += ",\"rele_restante\":" + (restante >= 0 ? String(restante) : String("null"));
  j += ",\"umidade_min\":" + String(cfg.umidMin);
  j += ",\"umidade_max\":" + String(cfg.umidMax);
  j += ",\"revezamento_min\":" + String(cfg.revMin);
  j += "}";
  enviarJson(j);
}

bool lerInt(const String& corpo, const char* chave, int& saida) {
  String k = String("\"") + chave + "\":";
  int i = corpo.indexOf(k);
  if (i < 0) return false;
  saida = corpo.substring(i + k.length()).toInt();
  return true;
}

void handleConfig() {
  String corpo = server.arg("plain");
  int v;
  if (lerInt(corpo, "umidadeMin", v)) cfg.umidMin = constrain(v, 0, 100);
  if (lerInt(corpo, "umidadeMax", v)) cfg.umidMax = constrain(v, 0, 100);
  if (lerInt(corpo, "revezamentoMinutos", v)) cfg.revMin = constrain(v, 1, 120);
  if (corpo.indexOf("\"modoPadrao\":\"automatico\"") >= 0) cfg.modoPadrao = 1;
  if (corpo.indexOf("\"modoPadrao\":\"manual\"") >= 0) cfg.modoPadrao = 0;
  salvarConfig();
  enviarJson("{\"ok\":true}");
}

void setup() {
  Serial.begin(115200);
  carregarConfig();
  modoAuto = (cfg.modoPadrao == 1);

  aplicarReles();                       // relés desligados antes de virar saída
  pinMode(RELE1_PIN, OUTPUT);
  pinMode(RELE2_PIN, OUTPUT);
  aplicarReles();

  bool usarRoteador = (strlen(WIFI_SSID) > 0);
  WiFi.mode(usarRoteador ? WIFI_AP_STA : WIFI_AP);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.print("\nRede criada: ");
  Serial.println(AP_SSID);
  Serial.print("Acesse: http://");
  Serial.println(WiFi.softAPIP());                // normalmente 192.168.4.1

  if (usarRoteador) {
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    unsigned long t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) { delay(400); }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("Também conectado ao roteador: http://");
      Serial.println(WiFi.localIP());
    }
  }
  if (MDNS.begin("irrigacao")) MDNS.addService("http", "tcp", 80);

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", PAGINA); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/ligar", HTTP_POST, []() { iniciarIrrigacao(); enviarJson("{\"ok\":true}"); });
  server.on("/api/desligar", HTTP_POST, []() { pararIrrigacao(); enviarJson("{\"ok\":true}"); });
  server.on("/api/automatico", HTTP_POST, []() { modoAuto = true; enviarJson("{\"ok\":true}"); });
  server.on("/api/manual", HTTP_POST, []() { modoAuto = false; enviarJson("{\"ok\":true}"); });
  server.on("/api/config", HTTP_POST, handleConfig);
  server.begin();

  lerSensor();
}

void loop() {
  server.handleClient();
  MDNS.update();

  if (millis() - ultimaLeitura >= 500) {
    ultimaLeitura = millis();
    lerSensor();
    logicaAutomatica();
  }
  revezarReles();
}
