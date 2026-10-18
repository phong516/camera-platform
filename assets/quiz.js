/* Shared quiz widget.
 * Markup contract:
 *   <div class="quiz">
 *     <div class="quiz-q" data-answer="1">
 *       <p class="quiz-prompt">Question…</p>
 *       <button class="quiz-opt">Option A</button>
 *       <button class="quiz-opt">Option B</button>
 *       <button class="quiz-opt">Option C</button>
 *       <p class="quiz-explain" data-explain>Why…</p>
 *     </div>
 *   </div>
 * data-answer is the 0-based index of the correct option.
 * Clicking a wrong option marks it and lets the learner retry (retrieval practice).
 * Clicking the correct option locks the question and reveals the explanation.
 */
(function () {
  "use strict";

  function setup(quiz) {
    var questions = quiz.querySelectorAll(".quiz-q");
    Array.prototype.forEach.call(questions, function (q) {
      var answer = parseInt(q.getAttribute("data-answer"), 10);
      var opts = q.querySelectorAll(".quiz-opt");
      var explain = q.querySelector(".quiz-explain");

      Array.prototype.forEach.call(opts, function (opt, i) {
        opt.addEventListener("click", function () {
          if (opt.disabled) return;
          if (i === answer) {
            opt.classList.add("correct");
            Array.prototype.forEach.call(opts, function (o) { o.disabled = true; });
            if (explain) explain.classList.add("show");
          } else {
            opt.classList.add("wrong");
            opt.disabled = true;
          }
        });
      });
    });
  }

  document.addEventListener("DOMContentLoaded", function () {
    var quizzes = document.querySelectorAll(".quiz");
    Array.prototype.forEach.call(quizzes, setup);
  });
})();
