#include <Trade\Trade.mqh>
#include <Trade\PositionInfo.mqh>
#include <Trade\OrderInfo.mqh>
#include <JAson.mqh>

CTrade trade;
CPositionInfo positionInfo;
COrderInfo orderInfo;

// Input Parameters
input group "=== Trading Parameters ==="
input string WebhookPort = "9000";
input double LotSize = 0.01;
input int SlippagePoints = 30;
input int MagicNumber = 123456;
input bool EnableLogging = true;

input group "=== Risk Management ==="
input double TP3_Offset = 5.0; // Additional pips for TP3 beyond TP1
input bool EnableTrailingStops = true;

input group "=== Order Management ==="
input int OrderExpirationHours = 24; // Hours before pending orders expire
input bool UseLimitOrders = true; // true = Limit Orders, false = Market Orders

input group "=== Webhook Configuration ==="
input string WebhookGetURL = "http://localhost:9000/webhook"; // URL to receive signals
input string WebhookUpdateURL = "http://localhost:9000/update"; // URL to send signal processing update
input int SignalCheckIntervalSeconds = 5; // How often to check for new signals
input bool EnableWebhookMode = true; // true = Web requests, false = simulation
input string WebhookToken = "your_secret_token"; // Security token for webhook validation

input group "=== Simulation Control ==="
input bool AutoRunSimulation = false; // Set to true to auto-run simulation on start

// Signal Structure
struct SignalParams {
   string signal;  // "BUY" or "SELL"
   double entry;   // Entry price
   double sl;      // Stop loss
   double tp1;     // First take profit
   double tp2;     // Second take profit
   string timestamp; // Signal timestamp
   string id;      // Unique signal ID
};

// Order State Tracking
struct OrderState {
   ulong ticket;
   bool isActive;
   bool isPosition; // true if converted to position, false if still pending
   bool tp1_hit;
   bool tp2_hit;
   bool sl_moved_to_entry;
   bool sl_moved_to_tp1;
};

// Global Variables
SignalParams sig;
bool ordersPlaced = false;
bool tradesOpened = false;
OrderState order1, order2, order3;
datetime lastSignalCheck = 0;
string lastProcessedSignalId = "";

// Logging function
void LogMessage(string message) {
   if (EnableLogging) {
      Print("[", TimeToString(TimeCurrent()), "] ", message);
   }
}

void OnTick() {
   // Check for new signals via webhook if enabled
   if (EnableWebhookMode && !ordersPlaced && !tradesOpened) {
      CheckForNewSignals();
   }
   
   if (!ordersPlaced && !tradesOpened) return;
   
   // Check if pending orders have been filled
   if (ordersPlaced && !tradesOpened) {
      CheckOrderFills();
   }
   
   // Update position states if trades are opened
   if (tradesOpened) {
      UpdatePositionStates();
      
      if (EnableTrailingStops) {
         double price = SymbolInfoDouble(_Symbol, sig.signal == "BUY" ? SYMBOL_BID : SYMBOL_ASK);
         
         if (sig.signal == "BUY") {
            HandleBuyTrailingStops(price);
         } else if (sig.signal == "SELL") {
            HandleSellTrailingStops(price);
         }
      }
   }
}

// NEW: Check for new signals via web request
void CheckForNewSignals() {
   // Throttle signal checking
   if (TimeCurrent() - lastSignalCheck < SignalCheckIntervalSeconds) {
      return;
   }
   
   lastSignalCheck = TimeCurrent();
   
   // Make web request to get latest signal
   string response = "";
   if (MakeWebRequest(response, WebhookGetURL)) {
     ProcessWebhookSignal(response);
   }
}

// NEW: Make HTTP request to webhook URL
bool MakeWebRequest(string &response, string webhookUrl) {
   string headers = "Content-Type: application/json\r\n";
   headers += "Authorization: Bearer " + WebhookToken + "\r\n";
   
   char data[];
   char result[];
   string resultHeaders;
   
   // Prepare request URL with timestamp to get latest signal
   string requestUrl = webhookUrl + "?timestamp=" + IntegerToString(TimeCurrent());

   // if (queryParams != "")
   //    requestUrl += "&" + queryParams;

   int timeout = 5000; // 5 second timeout
   int res = WebRequest("GET", requestUrl, headers, timeout, data, result, resultHeaders);
   
   if (res == -1) {
      int error = GetLastError();
      LogMessage("WebRequest failed. Error: " + IntegerToString(error));
      LogMessage("Make sure URL '" + requestUrl + "' is added to allowed URLs in Tools->Options->Expert Advisors");
      return false;
   }
   
   if (res == 200) {
      response = CharArrayToString(result);
      LogMessage("Received response: " + response);
      return true;
   } else {
      LogMessage("HTTP request failed with code: " + IntegerToString(res));
      return false;
   }
}

// NEW: Process incoming webhook signal
bool ProcessWebhookSignal(string jsonData) {
   if (StringLen(jsonData) == 0) {
      return false; // No data received
   }
   
   SignalParams newSignal;
   if (!ParseSignalFromJSON(jsonData, newSignal)) {
      LogMessage("Failed to parse signal from JSON");
      return false;
   }
   
   // Check if this is a new signal (avoid processing duplicates)
   if (newSignal.id == lastProcessedSignalId) {
      LogMessage("Signal already processed");
      return false; // Already processed this signal
   }
   
   // Validate the parsed signal
   if (!ValidateSignalParams(newSignal)) {
      LogMessage("Invalid signal parameters received");
      return false;
   }
   
   // Process the new signal
   sig = newSignal;
   lastProcessedSignalId = newSignal.id;
   
   LogMessage("Processing new " + sig.signal + " signal (ID: " + sig.id + ")");
   bool success = OpenSignalTrades(sig);
   
   if (success) {
      LogMessage("Signal processed successfully");
   } else {
      LogMessage("Failed to process signal");
   }
   
   return success;
}

// NEW: Parse signal from JSON data
bool ParseSignalFromJSON(string jsonData, SignalParams &signal) {
   // Reset signal structure
   signal.signal = "";
   signal.entry = 0;
   signal.sl = 0;
   signal.tp1 = 0;
   signal.tp2 = 0;
   signal.timestamp = "";
   signal.id = "";
   
   // Simple JSON parsing (you may want to use a more robust JSON library)
   if (StringFind(jsonData, "\"signal\"") == -1) {
      LogMessage("JSON missing 'signal' field");
      return false;
   }
   
   // Extract signal type
   signal.signal = ExtractJSONString(jsonData, "signal");
   if (signal.signal != "BUY" && signal.signal != "SELL") {
      LogMessage("Invalid signal type: " + signal.signal);
      return false;
   }
   
   // Extract price levels
   signal.entry = ExtractJSONDouble(jsonData, "entry");
   signal.sl = ExtractJSONDouble(jsonData, "sl");
   signal.tp1 = ExtractJSONDouble(jsonData, "tp1");
   signal.tp2 = ExtractJSONDouble(jsonData, "tp2");
   
   // Extract metadata
   signal.timestamp = ExtractJSONString(jsonData, "timestamp");
   signal.id = ExtractJSONString(jsonData, "page_id");
   
   // Validate required fields
   if (signal.entry == 0 || signal.sl == 0 || signal.tp1 == 0 || signal.tp2 == 0) {
      LogMessage("Missing required price levels in JSON");
      return false;
   }
   
   if (signal.id == "") {
      // Generate ID if not provided
      signal.id = IntegerToString(TimeCurrent()) + "_" + signal.signal;
   }
   
   LogMessage("Parsed signal: " + signal.signal + " Entry:" + DoubleToString(signal.entry, _Digits) + 
             " SL:" + DoubleToString(signal.sl, _Digits) + 
             " TP1:" + DoubleToString(signal.tp1, _Digits) + 
             " TP2:" + DoubleToString(signal.tp2, _Digits));
   
   return true;
}

// NEW: Helper function to extract string from JSON
string ExtractJSONString(string json, string key) {
   string searchPattern = "\"" + key + "\"";
   int startPos = StringFind(json, searchPattern);
   if (startPos == -1) return "";
   
   startPos = StringFind(json, ":", startPos);
   if (startPos == -1) return "";
   
   startPos = StringFind(json, "\"", startPos);
   if (startPos == -1) return "";
   startPos++; // Move past opening quote
   
   int endPos = StringFind(json, "\"", startPos);
   if (endPos == -1) return "";
   
   return StringSubstr(json, startPos, endPos - startPos);
}

// NEW: Helper function to extract double from JSON
double ExtractJSONDouble(string json, string key) {
   string searchPattern = "\"" + key + "\"";
   int startPos = StringFind(json, searchPattern);
   if (startPos == -1) return 0;
   
   startPos = StringFind(json, ":", startPos);
   if (startPos == -1) return 0;
   
   // Skip whitespace and quotes
   startPos++;
   while (startPos < StringLen(json) && (StringGetCharacter(json, startPos) == ' ' || 
          StringGetCharacter(json, startPos) == '"')) {
      startPos++;
   }
   
   // Find end of number
   int endPos = startPos;
   while (endPos < StringLen(json)) {
      ushort ch = StringGetCharacter(json, endPos);
      if (ch != '.' && ch != '-' && (ch < '0' || ch > '9')) {
         break;
      }
      endPos++;
   }
   
   if (endPos > startPos) {
      string numberStr = StringSubstr(json, startPos, endPos - startPos);
      return StringToDouble(numberStr);
   }
   
   return 0;
}


void CheckOrderFills() {
   // Check if orders have been converted to positions
   bool order1_filled = false;
   bool order2_filled = false;
   bool order3_filled = false;
   
   // Check each order
   if (order1.isActive && !order1.isPosition) {
      if (!OrderSelect(order1.ticket)) {
         // Order no longer exists, check if it became a position
         if (PositionSelectByTicket(order1.ticket)) {
            order1.isPosition = true;
            order1_filled = true;
            LogMessage("Order 1 filled and converted to position: " + IntegerToString(order1.ticket));
         } else {
            order1.isActive = false;
            LogMessage("Order 1 expired or cancelled: " + IntegerToString(order1.ticket));
         }
      }
   }
   
   if (order2.isActive && !order2.isPosition) {
      if (!OrderSelect(order2.ticket)) {
         if (PositionSelectByTicket(order2.ticket)) {
            order2.isPosition = true;
            order2_filled = true;
            LogMessage("Order 2 filled and converted to position: " + IntegerToString(order2.ticket));
         } else {
            order2.isActive = false;
            LogMessage("Order 2 expired or cancelled: " + IntegerToString(order2.ticket));
         }
      }
   }
   
   if (order3.isActive && !order3.isPosition) {
      if (!OrderSelect(order3.ticket)) {
         if (PositionSelectByTicket(order3.ticket)) {
            order3.isPosition = true;
            order3_filled = true;
            LogMessage("Order 3 filled and converted to position: " + IntegerToString(order3.ticket));
         } else {
            order3.isActive = false;
            LogMessage("Order 3 expired or cancelled: " + IntegerToString(order3.ticket));
         }
      }
   }
   
   // If any orders filled, mark trades as opened
   if (order1_filled || order2_filled || order3_filled) {
      tradesOpened = true;
      LogMessage("At least one order filled. Trailing stops now active.");
   }
   
   // If all orders are no longer pending, reset ordersPlaced
   if (!order1.isActive && !order2.isActive && !order3.isActive) {
      ordersPlaced = false;
      LogMessage("All pending orders processed.");
   }
}

void HandleBuyTrailingStops(double price) {
   // When price reaches TP1, move SL of positions 2 & 3 to breakeven
   if (price >= sig.tp1 && !order2.sl_moved_to_entry && !order3.sl_moved_to_entry) {
      if (order2.isPosition && UpdateSL(order2.ticket, sig.entry)) {
         order2.sl_moved_to_entry = true;
         LogMessage("BUY: Moved SL to breakeven for position 2 (ticket: " + IntegerToString(order2.ticket) + ")");
      }
      if (order3.isPosition && UpdateSL(order3.ticket, sig.entry)) {
         order3.sl_moved_to_entry = true;
         LogMessage("BUY: Moved SL to breakeven for position 3 (ticket: " + IntegerToString(order3.ticket) + ")");
      }
   }
   
   // When price reaches TP2, move SL of position 3 to TP1
   if (price >= sig.tp2 && !order3.sl_moved_to_tp1) {
      if (order3.isPosition && UpdateSL(order3.ticket, sig.tp1)) {
         order3.sl_moved_to_tp1 = true;
         LogMessage("BUY: Moved SL to TP1 for position 3 (ticket: " + IntegerToString(order3.ticket) + ")");
      }
   }
}

void HandleSellTrailingStops(double price) {
   // When price reaches TP1, move SL of positions 2 & 3 to breakeven
   if (price <= sig.tp1 && !order2.sl_moved_to_entry && !order3.sl_moved_to_entry) {
      if (order2.isPosition && UpdateSL(order2.ticket, sig.entry)) {
         order2.sl_moved_to_entry = true;
         LogMessage("SELL: Moved SL to breakeven for position 2 (ticket: " + IntegerToString(order2.ticket) + ")");
      }
      if (order3.isPosition && UpdateSL(order3.ticket, sig.entry)) {
         order3.sl_moved_to_entry = true;
         LogMessage("SELL: Moved SL to breakeven for position 3 (ticket: " + IntegerToString(order3.ticket) + ")");
      }
   }
   
   // When price reaches TP2, move SL of position 3 to TP1
   if (price <= sig.tp2 && !order3.sl_moved_to_tp1) {
      if (order3.isPosition && UpdateSL(order3.ticket, sig.tp1)) {
         order3.sl_moved_to_tp1 = true;
         LogMessage("SELL: Moved SL to TP1 for position 3 (ticket: " + IntegerToString(order3.ticket) + ")");
      }
   }
}

void UpdatePositionStates() {
   // Update position status for filled orders
   if (order1.isPosition) {
      order1.isActive = PositionSelectByTicket(order1.ticket);
   }
   if (order2.isPosition) {
      order2.isActive = PositionSelectByTicket(order2.ticket);
   }
   if (order3.isPosition) {
      order3.isActive = PositionSelectByTicket(order3.ticket);
   }
   
   // Check if all positions are closed
   if (order1.isPosition && order2.isPosition && order3.isPosition &&
       !order1.isActive && !order2.isActive && !order3.isActive) {
      tradesOpened = false;
      ordersPlaced = false;
      LogMessage("All positions closed. Ready for new signal.");
   }
}

bool UpdateSL(ulong ticket, double new_sl) {
   if (!PositionSelectByTicket(ticket)) {
      LogMessage("Error: Position not found for ticket " + IntegerToString(ticket));
      return false;
   }
   
   double current_sl = PositionGetDouble(POSITION_SL);
   double tp = PositionGetDouble(POSITION_TP);
   
   // Avoid unnecessary modifications
   if (MathAbs(current_sl - new_sl) < _Point) {
      return true; // Already at target SL
   }
   
   // Validate SL direction
   ENUM_POSITION_TYPE pos_type = (ENUM_POSITION_TYPE)PositionGetInteger(POSITION_TYPE);
   if (pos_type == POSITION_TYPE_BUY && new_sl <= current_sl) {
      return true; // Don't move SL backwards for BUY
   }
   if (pos_type == POSITION_TYPE_SELL && new_sl >= current_sl) {
      return true; // Don't move SL backwards for SELL
   }
   
   bool result = trade.PositionModify(ticket, new_sl, tp);
   if (!result) {
      LogMessage("Error modifying position " + IntegerToString(ticket) + ": " + 
                IntegerToString(trade.ResultRetcode()) + " - " + trade.ResultRetcodeDescription());
   }
   return result;
}

bool OpenSignalTrades(SignalParams &s) {
   // Validate signal parameters
   if (!ValidateSignalParams(s)) {
      LogMessage("Error: Invalid signal parameters");
      return false;
   }
   
   trade.SetExpertMagicNumber(MagicNumber);
   trade.SetDeviationInPoints(SlippagePoints);
   
   // Calculate TP3 based on signal direction
   double tp3;
   if (s.signal == "BUY") {
      tp3 = s.tp1 + TP3_Offset * _Point;
   } else {
      tp3 = s.tp1 - TP3_Offset * _Point;
   }
   
   // Calculate expiration time
   datetime expiration = TimeCurrent() + OrderExpirationHours * 3600;
   
   LogMessage("Placing " + s.signal + " " + (UseLimitOrders ? "LIMIT" : "MARKET") + 
             " orders at " + DoubleToString(s.entry, _Digits));
   
   bool success = false;
   
   if (UseLimitOrders) {
      success = PlaceLimitOrders(s, tp3, expiration);
   } else {
      success = PlaceMarketOrders(s, tp3);
   }
   
   if (success) {
      InitializeOrderStates();
      LogMessage("Successfully placed 3 orders: " + 
                IntegerToString(order1.ticket) + ", " + 
                IntegerToString(order2.ticket) + ", " + 
                IntegerToString(order3.ticket));

      // send webhook request to update database (order is processed)
      string r = "";
      if(MakeWebRequest(r, WebhookUpdateURL)) {
         LogMessage("send order update to database");
      }
      
   }
   
   return success;
}

bool PlaceLimitOrders(SignalParams &s, double tp3, datetime expiration) {
   ENUM_ORDER_TYPE orderType = s.signal == "BUY" ? ORDER_TYPE_BUY_LIMIT : ORDER_TYPE_SELL_LIMIT;
   
   // Place limit orders
   order1.ticket = trade.OrderOpen(_Symbol, orderType, LotSize, 0, s.entry, s.sl, s.tp1, 
                                  ORDER_TIME_SPECIFIED, expiration, "TP1 Limit Order");
   if (order1.ticket == 0) {
      LogMessage("Failed to place limit order 1: " + IntegerToString(trade.ResultRetcode()));
      return false;
   }
   
   order2.ticket = trade.OrderOpen(_Symbol, orderType, LotSize, 0, s.entry, s.sl, s.tp2, 
                                  ORDER_TIME_SPECIFIED, expiration, "TP2 Limit Order");
   if (order2.ticket == 0) {
      LogMessage("Failed to place limit order 2: " + IntegerToString(trade.ResultRetcode()));
      trade.OrderDelete(order1.ticket);
      return false;
   }
   
   order3.ticket = trade.OrderOpen(_Symbol, orderType, LotSize, 0, s.entry, s.sl, tp3, 
                                  ORDER_TIME_SPECIFIED, expiration, "TP3 Limit Order");
   if (order3.ticket == 0) {
      LogMessage("Failed to place limit order 3: " + IntegerToString(trade.ResultRetcode()));
      trade.OrderDelete(order1.ticket);
      trade.OrderDelete(order2.ticket);
      return false;
   }
   
   ordersPlaced = true;
   return true;
}

bool PlaceMarketOrders(SignalParams &s, double tp3) {
   ENUM_ORDER_TYPE type = s.signal == "BUY" ? ORDER_TYPE_BUY : ORDER_TYPE_SELL;
   
   // Place market orders (original behavior)
   order1.ticket = trade.PositionOpen(_Symbol, type, LotSize, s.entry, s.sl, s.tp1, "TP1 Trade");
   if (order1.ticket == 0) {
      LogMessage("Failed to open position 1: " + IntegerToString(trade.ResultRetcode()));
      return false;
   }
   
   order2.ticket = trade.PositionOpen(_Symbol, type, LotSize, s.entry, s.sl, s.tp2, "TP2 Trade");
   if (order2.ticket == 0) {
      LogMessage("Failed to open position 2: " + IntegerToString(trade.ResultRetcode()));
      trade.PositionClose(order1.ticket);
      return false;
   }
   
   order3.ticket = trade.PositionOpen(_Symbol, type, LotSize, s.entry, s.sl, tp3, "TP3 Trade");
   if (order3.ticket == 0) {
      LogMessage("Failed to open position 3: " + IntegerToString(trade.ResultRetcode()));
      trade.PositionClose(order1.ticket);
      trade.PositionClose(order2.ticket);
      return false;
   }
   
   // Mark as positions (not pending orders)
   order1.isPosition = true;
   order2.isPosition = true;
   order3.isPosition = true;
   tradesOpened = true;
   
   return true;
}

void InitializeOrderStates() {
   order1.isActive = true;
   order1.isPosition = !UseLimitOrders; // Market orders are immediately positions
   order1.tp1_hit = false;
   order1.tp2_hit = false;
   order1.sl_moved_to_entry = false;
   order1.sl_moved_to_tp1 = false;
   
   order2.isActive = true;
   order2.isPosition = !UseLimitOrders;
   order2.tp1_hit = false;
   order2.tp2_hit = false;
   order2.sl_moved_to_entry = false;
   order2.sl_moved_to_tp1 = false;
   
   order3.isActive = true;
   order3.isPosition = !UseLimitOrders;
   order3.tp1_hit = false;
   order3.tp2_hit = false;
   order3.sl_moved_to_entry = false;
   order3.sl_moved_to_tp1 = false;
}

bool ValidateSignalParams(SignalParams &s) {
   if (s.signal != "BUY" && s.signal != "SELL") {
      LogMessage("Invalid signal type: " + s.signal);
      return false;
   }
   
   double currentPrice = SymbolInfoDouble(_Symbol, s.signal == "BUY" ? SYMBOL_ASK : SYMBOL_BID);
   
   if (s.signal == "BUY") {
      if (s.sl >= s.entry || s.tp1 <= s.entry || s.tp2 <= s.tp1) {
         LogMessage("Invalid BUY signal levels");
         return false;
      }
      // For limit orders, entry should be below current price
      if (UseLimitOrders && s.entry >= currentPrice) {
         LogMessage("BUY limit order entry price should be below current price");
         return false;
      }
   } else {
      if (s.sl <= s.entry || s.tp1 >= s.entry || s.tp2 >= s.tp1) {
         LogMessage("Invalid SELL signal levels");
         return false;
      }
      // For limit orders, entry should be above current price
      if (UseLimitOrders && s.entry <= currentPrice) {
         LogMessage("SELL limit order entry price should be above current price");
         return false;
      }
   }
   
   return true;
}

void SimulateIncomingSignal() {
   double currentPrice = SymbolInfoDouble(_Symbol, SYMBOL_BID);
   
   if (UseLimitOrders) {
      // Example BUY LIMIT signal (entry below current price)
      sig.signal = "BUY";
      sig.entry = 3320 - 50 * _Point; // 5 pips below current price
      sig.sl = 3310 - 50 * _Point;       // 5 pips below entry
      sig.tp1 = 3321 + 100 * _Point;     // 10 pips above entry
      sig.tp2 = 3322 + 200 * _Point;     // 20 pips above entry
      
      /* Example SELL LIMIT signal (entry above current price)
      sig.signal = "SELL";
      sig.entry = currentPrice + 50 * _Point; // 5 pips above current price
      sig.sl = sig.entry + 50 * _Point;       // 5 pips above entry
      sig.tp1 = sig.entry - 100 * _Point;     // 10 pips below entry
      sig.tp2 = sig.entry - 200 * _Point;     // 20 pips below entry
      */
   } else {
      // Original market order example
      sig.signal = "BUY";
      sig.entry = 3329.50;
      sig.sl = 3320.00;
      sig.tp1 = 3330.50;
      sig.tp2 = 3332.50;
   }
   
   LogMessage("Processing " + sig.signal + " signal");
   bool success = OpenSignalTrades(sig);
   
   if (success) {
      LogMessage("Signal processed successfully");
   } else {
      LogMessage("Failed to process signal");
   }
}

// Clean up function
void CloseAllPositions() {
   // Close positions
   if (order1.isPosition && order1.isActive) trade.PositionClose(order1.ticket);
   if (order2.isPosition && order2.isActive) trade.PositionClose(order2.ticket);
   if (order3.isPosition && order3.isActive) trade.PositionClose(order3.ticket);
   
   // Cancel pending orders
   if (!order1.isPosition && order1.isActive) trade.OrderDelete(order1.ticket);
   if (!order2.isPosition && order2.isActive) trade.OrderDelete(order2.ticket);
   if (!order3.isPosition && order3.isActive) trade.OrderDelete(order3.ticket);
   
   tradesOpened = false;
   ordersPlaced = false;
   LogMessage("All positions and orders closed/cancelled manually");
}

int OnInit() {
   LogMessage("Enhanced Gold Processor EA initialized");
   LogMessage("Order Type: " + (UseLimitOrders ? "LIMIT ORDERS" : "MARKET ORDERS"));
   LogMessage("Webhook Port: " + WebhookPort);
   LogMessage("Lot Size: " + DoubleToString(LotSize, 2));
   LogMessage("Magic Number: " + IntegerToString(MagicNumber));
   LogMessage("Order Expiration: " + IntegerToString(OrderExpirationHours) + " hours");
   
   // Initialize trade object
   trade.SetExpertMagicNumber(MagicNumber);
   trade.SetMarginMode();
   trade.SetTypeFillingBySymbol(_Symbol);
   
   // Only run simulation if webhook mode is disabled or auto-simulation is enabled
   if (!EnableWebhookMode && AutoRunSimulation) {
      LogMessage("Running simulation (webhook mode disabled)...");
      SimulateIncomingSignal();
   } else if (EnableWebhookMode) {
      LogMessage("Webhook mode enabled. Waiting for signals from: " + WebhookGetURL);
   } else {
      LogMessage("EA ready. Set EnableWebhookMode=true or AutoRunSimulation=true to activate.");
   }
   return INIT_SUCCEEDED;
}

void OnDeinit(const int reason) {
   LogMessage("EA deinitialized. Reason: " + IntegerToString(reason));
}

void OnTradeTransaction(const MqlTradeTransaction& trans,
                       const MqlTradeRequest& request,
                       const MqlTradeResult& result) {
   if (EnableLogging && trans.symbol == _Symbol) {
      LogMessage("Trade transaction: " + EnumToString(trans.type) + 
                " for ticket " + IntegerToString(trans.order));
   }
}