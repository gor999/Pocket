# SMS samples

**Every SMS in `tests/tst_SmsParsing.cpp`, `tst_SmsImport.cpp` and the demo data
(`src/core/DemoData.cpp`) is synthetic.** I wrote them to exercise the parsers
(amount formats, three languages, income and expense, missing merchant, unknown
bank, malformed amounts, OTPs). They are **not** copies of real IDBank, Ameriabank
or ACBA messages: no real samples were available in the development environment,
and the banks' actual wording was not found in public sources.

What a passing test therefore means: the parsing *machinery* works on the layouts
I invented, and the bank-agnostic fallback reads a range of other layouts at lower
confidence. It does **not** show that real bank messages parse. Until real samples
are added, treat the three bank parsers as unverified.

## Adding real samples (please do this first)

1. Pick 5-10 real transaction SMS per bank in each language you receive (purchase,
   cash withdrawal, incoming transfer, salary, refund, foreign currency).
2. **Anonymise** them: replace card digits with `*1234`-style last four (or invented
   digits), and change names or places if you do not want them kept. Never commit
   OTPs, full card numbers, CVV, PINs, or account numbers.
3. Add a row per message to `realSamples_data()` in `tests/tst_SmsParsing.cpp`
   (body, expected bank, amount in minor units, income/expense). The test is
   skipped while the table is empty.
4. Run `ctest --test-dir build -R tst_SmsParsing`. Rows that fail show exactly what
   the bank writes that the templates do not cover; fix the template layouts in
   `src/sms/<Bank>SmsParser.cpp` (one string per language) and re-run.

Messages the app could not read on your iPhone are listed in
**Settings → Bank SMS import → Could not import**, with the (masked) text.
