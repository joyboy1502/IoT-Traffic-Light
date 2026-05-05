// ── Pin Mapping ───────────────────────────────────────────────
// Utara
#define PIN_U_RED  25
#define PIN_U_YEL  26
#define PIN_U_GRN  27

// Selatan
#define PIN_S_RED  14
#define PIN_S_YEL  12
#define PIN_S_GRN  13

// Timur
#define PIN_T_RED  33
#define PIN_T_YEL  23
#define PIN_T_GRN  22

// Barat
#define PIN_B_RED  18
#define PIN_B_YEL  19
#define PIN_B_GRN  21

// ── Durasi (ms) ───────────────────────────────────────────────
#define DUR_RED  2000   // 2 detik
#define DUR_YEL  1000   // 1 detik
#define DUR_GRN  2000   // 2 detik

// ── State Machine ─────────────────────────────────────────────
enum Phase { PHASE_NS_GREEN, PHASE_NS_YELLOW, PHASE_EW_GREEN, PHASE_EW_YELLOW };
Phase currentPhase = PHASE_NS_GREEN;
unsigned long phaseStart = 0;

// ─────────────────────────────────────────────────────────────
//  Traffic Light Helpers
// ─────────────────────────────────────────────────────────────
struct TrafficPins { uint8_t red, yel, grn; };

TrafficPins UTARA   = { PIN_U_RED, PIN_U_YEL, PIN_U_GRN };
TrafficPins SELATAN = { PIN_S_RED, PIN_S_YEL, PIN_S_GRN };
TrafficPins TIMUR   = { PIN_T_RED, PIN_T_YEL, PIN_T_GRN };
TrafficPins BARAT   = { PIN_B_RED, PIN_B_YEL, PIN_B_GRN };

void setLight(TrafficPins tp, bool red, bool yel, bool grn) {
  digitalWrite(tp.red, red  ? HIGH : LOW);
  digitalWrite(tp.yel, yel  ? HIGH : LOW);
  digitalWrite(tp.grn, grn  ? HIGH : LOW);
}

void allRed() {
  setLight(UTARA,   true, false, false);
  setLight(SELATAN, true, false, false);
  setLight(TIMUR,   true, false, false);
  setLight(BARAT,   true, false, false);
}

void applyPhase(Phase p) {
  switch (p) {
    case PHASE_NS_GREEN:
      setLight(UTARA,   false, false, true);
      setLight(SELATAN, false, false, true);
      setLight(TIMUR,   true,  false, false);
      setLight(BARAT,   true,  false, false);
      Serial.println("[FASE 1] U+S=HIJAU | T+B=MERAH");
      break;

    case PHASE_NS_YELLOW:
      setLight(UTARA,   false, true,  false);
      setLight(SELATAN, false, true,  false);
      setLight(TIMUR,   true,  false, false);
      setLight(BARAT,   true,  false, false);
      Serial.println("[FASE 2] U+S=KUNING | T+B=MERAH");
      break;

    case PHASE_EW_GREEN:
      setLight(UTARA,   true,  false, false);
      setLight(SELATAN, true,  false, false);
      setLight(TIMUR,   false, false, true);
      setLight(BARAT,   false, false, true);
      Serial.println("[FASE 3] T+B=HIJAU | U+S=MERAH");
      break;

    case PHASE_EW_YELLOW:
      setLight(UTARA,   true,  false, false);
      setLight(SELATAN, true,  false, false);
      setLight(TIMUR,   false, true,  false);
      setLight(BARAT,   false, true,  false);
      Serial.println("[FASE 4] T+B=KUNING | U+S=MERAH");
      break;
  }
}

unsigned long phaseDuration(Phase p) {
  switch (p) {
    case PHASE_NS_GREEN:  return DUR_GRN;
    case PHASE_NS_YELLOW: return DUR_YEL;
    case PHASE_EW_GREEN:  return DUR_GRN;
    case PHASE_EW_YELLOW: return DUR_YEL;
    default: return DUR_RED;
  }
}

Phase nextPhase(Phase p) {
  switch (p) {
    case PHASE_NS_GREEN:  return PHASE_NS_YELLOW;
    case PHASE_NS_YELLOW: return PHASE_EW_GREEN;
    case PHASE_EW_GREEN:  return PHASE_EW_YELLOW;
    case PHASE_EW_YELLOW: return PHASE_NS_GREEN;
    default:              return PHASE_NS_GREEN;
  }
}

// ─────────────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial.println("=== Traffic Light Tugas 2 ===");

  // Init semua pin sebagai OUTPUT
  uint8_t pins[] = {
    PIN_U_RED, PIN_U_YEL, PIN_U_GRN,
    PIN_S_RED, PIN_S_YEL, PIN_S_GRN,
    PIN_T_RED, PIN_T_YEL, PIN_T_GRN,
    PIN_B_RED, PIN_B_YEL, PIN_B_GRN
  };
  for (uint8_t p : pins) {
    pinMode(p, OUTPUT);
    digitalWrite(p, LOW);
  }

  // Semua merah dulu saat start
  allRed();
  delay(1000);

  // Mulai fase pertama
  currentPhase = PHASE_NS_GREEN;
  phaseStart   = millis();
  applyPhase(currentPhase);
}

// ─────────────────────────────────────────────────────────────
//  Loop
// ─────────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();

  // Cek apakah durasi fase sekarang sudah habis
  if (now - phaseStart >= phaseDuration(currentPhase)) {
    currentPhase = nextPhase(currentPhase);
    phaseStart   = now;
    applyPhase(currentPhase);
  }
}
